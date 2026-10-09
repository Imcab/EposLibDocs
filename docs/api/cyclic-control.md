# Cyclic control

In the cyclic synchronous modes the trajectory is generated **by your program**: a new
setpoint every SYNC period, applied by every drive on the same SYNC. This is the path for
control loops - trajectory followers, wheel velocity control, torque control.

## The lock-free path

Everything else on `Epos4` posts work to the driver thread and waits. The cyclic path does
not:

```text
 control thread                    bus thread
 ──────────────                    ──────────
 StageTargetVelocity(v) ─ store ─▶ OnSync: staged target → RPDO → bus
 GetCachedVelocity()    ◀─ load ── OnRpdoWrite: TPDO → cached feedback
```

Staging a setpoint is an atomic store; the bus thread publishes it on the next SYNC.
Reading feedback is an atomic load of what the last TPDO carried. No system call, no lock,
no waiting - safe in a real-time loop, and from a different thread than the bus.

## Entering and leaving

```cpp
motor.Enable();                                // the drive must be enabled first
if (auto ec = motor.EnterCyclicVelocityMode()) {
  // not_connected, not_supported, or the drive refused the mode
}
// ... loop: StageTargetVelocity() every period ...
motor.ExitCyclicMode();
motor.Disable();
```

| | Mode | Publishes on every SYNC |
|---|---|---|
| `EnterCyclicPositionMode()` | CSP | Controlword + Target position `0x607A` |
| `EnterCyclicVelocityMode()` | CSV | Controlword + Target velocity `0x60FF` |
| `EnterCyclicTorqueMode()` | CST | Controlword + Target torque `0x6071` |

`Enter*Mode()` switches the mode **once** - checking and writing `0x6060` is an SDO round
trip, which inside a loop would be the whole budget - and seeds the setpoint so the first
SYNC holds the axis where it is:

- **CSP** starts at the actual position;
- **CSV** starts at 0 rpm;
- **CST** starts at the torque the axis is producing now - zero would let go of a loaded
  joint - and reads the motor's rated torque once, so torques in N m can be staged without
  bus access.

It does **not** walk the state machine: call `Enable()` first. `ExitCyclicMode()` stops
publishing; the drive stays in its mode and state - use `Disable()` to remove power.
`IsCyclicModeActive()` says whether the path is publishing.

## Staging setpoints

```cpp
bool StageTargetPosition(PositionSetpoint position);   // quadcounts, or an angle at the output
bool StageTargetVelocity(VelocitySetpoint velocity);   // rpm at the motor, or at the output
bool StageTargetTorque(TorqueSetpoint torque);         // thousandths of rated torque, or N m
```

```cpp
motor.StageTargetPosition(50000);         // quadcounts
motor.StageTargetPosition(90_deg);        // needs SetMechanism()
motor.StageTargetVelocity(1500);          // rpm at the motor
motor.StageTargetVelocity(15_rpm);        // at the output, needs SetMechanism()
motor.StageTargetTorque(std::int16_t{250});   // 25 % of rated torque
motor.StageTargetTorque(0.5_Nm);          // at the motor shaft
```

Each returns **false**, and stages nothing, when a quantity cannot be converted: no
mechanism set, or for torque, no rated torque (motor data not configured, or not in torque
mode). Conversion is arithmetic only - no bus access. Staging a setpoint the active mode
does not publish is harmless; it has no effect until that mode is entered.

!!! note "Plain numbers are raw"
    `StageTargetTorque(250)` stages 250 thousandths of the rated torque - 25 % - not 250 N m.
    A torque in newton metres is always a quantity: `0.25_Nm`.

## Reading feedback

```cpp
std::int32_t  GetCachedPosition() const;     // 0x6064 [quadcounts]
std::int32_t  GetCachedVelocity() const;     // 0x606C [rpm]
std::int16_t  GetCachedTorque() const;       // 0x6077 [thousandths of rated torque]
std::uint16_t GetCachedStatusword() const;   // 0x6041
std::uint16_t GetCachedErrorCode() const;    // last EMCY, 0 if none or reset
```

These are the values from the last TPDO - at most one SYNC period old - and they work with
or without the cyclic mode active, as long as PDOs flow.

## Health: check it every cycle

```cpp
bool IsCyclicHealthy(std::chrono::steady_clock::duration maxAge = 50ms) const;
```

True while PDOs keep arriving (the last one within `maxAge`) **and** the drive reports
Operation enabled. When the bus goes quiet the cached values stop changing but keep reading
back happily, so a controller that does not check would go on believing a dead axis is
tracking. At a 10 ms SYNC, 50 ms is five missed periods.

```cpp
if (!motor.IsCyclicHealthy()) {
  // the drive faulted, lost power, or stopped talking
}
```

## A complete loop

```cpp
motor.Enable();
motor.EnterCyclicVelocityMode();

constexpr auto kPeriod = 10ms;                 // = sync_period
auto next = std::chrono::steady_clock::now();
while (running) {
  motor.StageTargetVelocity(ComputeTarget(motor.GetCachedVelocity()));
  if (!motor.IsCyclicHealthy()) {
    break;
  }
  next += kPeriod;
  std::this_thread::sleep_until(next);
}

motor.StageTargetVelocity(0);                  // brake with the velocity loop
// ... wait for |velocity| to drop ...
motor.ExitCyclicMode();
motor.Disable();
```

Complete, tested programs: [cyclic velocity](../examples/cyclic-velocity.md),
[cyclic torque](../examples/cyclic-torque.md), [cyclic position](../examples/cyclic-position.md).

## Position (CSP)

The drive's position loop follows your trajectory, interpolating between setpoints over the
**interpolation time period** - set it to the SYNC period:

```cpp
epos4::configs::CyclicConfigs cyclic;
cyclic.interpolationTimePeriodMs = 10;
motor.GetConfigurator().Apply(cyclic);
```

Steps in the setpoint larger than the drive can follow raise a following error
(`HasFollowingError()`, fault `0x8611`) - generate a smooth trajectory, and size
`LimitConfigs::followingErrorWindow` for it.

## Velocity (CSV)

The drive's velocity loop follows your velocity profile, and **brakes actively** toward a
zero target - unlike CST, where zero torque lets the axis coast. The usual choice for wheels.

## Torque (CST) { #torque }

The drive's current loop produces the torque you stage; the velocity is whatever physics
makes of it.

- **Everything is relative to «Motor rated torque»** (`0x6076` = nominal current × torque
  constant), at the **motor shaft**. If the motor data was never configured it is 0, and
  N m conversions fail. Check it first:
  ```cpp
  auto & rated = motor.GetMotorRatedTorque().Refresh();   // µN m
  if (rated.GetStatus() || rated.GetValue() == 0) { /* configure MotorConfigs */ }
  ```
- **Converters**, which refresh the rated torque if it has not been read:
  ```cpp
  std::optional<std::int16_t> pm = motor.TorqueToPerThousand(0.1_Nm);
  std::optional<units::torque::newton_meter_t> t = motor.PerThousandToTorque(150);
  ```
- **A free shaft accelerates.** A constant torque on an unloaded motor produces constant
  acceleration, until the motor reaches its no-load speed (about the speed constant × the
  supply voltage) and the torque collapses. Guard the loop with a velocity limit, or test
  against a blocked shaft.
- **Zero torque is not a brake.** The axis coasts. To stop a moving axis, switch to CSV with
  a zero target, or `QuickStop()`.

## Offsets and feed-forward

The cyclic requests carry optional offsets, added by the drive to the target:

```cpp
motor.SetControl(epos4::controls::CyclicPosition{}
                   .WithPosition(target)
                   .WithTorqueOffset(0.12_Nm));     // 0x60B2: gravity compensation
motor.SetControl(epos4::controls::CyclicVelocity{}
                   .WithVelocity(800)
                   .WithVelocityOffset(50));        // 0x60B1
motor.SetControl(epos4::controls::CyclicTorque{}
                   .WithTorque(0.2_Nm)
                   .WithTorqueOffset(0.05_Nm));
```

`SetControl()` with a cyclic request resolves everything first, switches the mode if needed,
writes the offsets over SDO and puts the target in the RPDO. It costs SDO round trips, so
use it to set offsets occasionally - a torque offset that changes with the arm's pose, a few
times a second - and `StageTarget*()` for the per-cycle target.
