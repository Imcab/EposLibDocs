# Control requests

A control request is a small struct describing **what** the drive should do: one per CiA 402
operating mode. `SetControl()` switches the drive into the mode the request needs - doing
nothing if it is already there - and commands it.

```cpp
std::error_code SetControl(const controls::ProfilePosition & request);
std::error_code SetControl(const controls::ProfileVelocity & request);
std::error_code SetControl(const controls::CyclicPosition & request);
std::error_code SetControl(const controls::CyclicVelocity & request);
std::error_code SetControl(const controls::CyclicTorque & request);
std::error_code SetControl(const controls::Halt & request);
bool Home(const controls::Homing & request, std::chrono::milliseconds timeout = 30s);
```

The drive must be **enabled** first; see [Enabling and stopping](enabling.md).

| Request | Mode | Trajectory | Page |
|---|---|---|---|
| `ProfilePosition` | PPM (1) | the drive ramps to a target position | this page |
| `ProfileVelocity` | PVM (3) | the drive ramps to a target velocity | this page |
| `Homing` | HMM (6) | the drive runs a homing method | [Homing](homing.md) |
| `CyclicPosition` | CSP (8) | your program, one setpoint per SYNC | [Cyclic control](cyclic-control.md) |
| `CyclicVelocity` | CSV (9) | your program | [Cyclic control](cyclic-control.md) |
| `CyclicTorque` | CST (10) | your program | [Cyclic control](cyclic-control.md) |
| `Halt` | - | stop in the current mode, stay enabled | [Enabling and stopping](enabling.md#halt) |

Before switching, `SetControl()` checks the drive's «Supported drive modes» (`0x6502`) and
returns `std::errc::not_supported` for a mode it does not implement. After writing the mode
it confirms it through «Modes of operation display» (`0x6061`), so a refused switch never
goes unnoticed. `motor.SupportsMode(mode)` asks the same question up front.

## Profile Position

The drive generates a trapezoidal ramp to a target; you send the endpoint.

```cpp
struct ProfilePosition
{
  PositionSetpoint position{0};               // 0x607A: quadcounts, or an angle
  std::optional<SpeedSetpoint> velocity;      // 0x6081: rpm, or an angular velocity
  std::optional<std::uint32_t> acceleration;  // 0x6083: rpm/s
  std::optional<std::uint32_t> deceleration;  // 0x6084: rpm/s
  bool relative{false};                       // Controlword bit 6
  bool changeSetImmediately{false};           // Controlword bit 5
};
```

```cpp
// Absolute, raw units, using the profile configured in MotionProfileConfigs.
motor.SetControl(epos4::controls::ProfilePosition{}.WithPosition(50000));

// 90 degrees further at the output, at 1500 rpm motor speed.
motor.SetControl(epos4::controls::ProfilePosition{}
                   .WithPosition(90_deg)
                   .WithVelocity(1500)
                   .WithAcceleration(8000)
                   .WithDeceleration(8000)
                   .WithRelative(true));
```

- **Unset overrides use the drive's configuration.** Set the profile once with
  `MotionProfileConfigs` and requests do not have to repeat it - and do not pay an SDO write
  per move for it.
- **`relative`** moves from the current target instead of to an absolute position.
  Absolute is what an arm normally wants: relative moves accumulate whatever error the
  previous move left.
- **`changeSetImmediately`**: `false` finishes the move in progress, then starts this one;
  `true` aborts it and starts at once.
- **The velocity is a magnitude.** The direction comes from the target position.

### The setpoint handshake

A PPM target is taken on a handshake (Table 3-15): raise *New setpoint* (Controlword bit 4),
wait for *Setpoint acknowledge* (Statusword bit 12), lower bit 4 again. `SetControl()` does
all of it, and returns once the drive has **accepted** the target - not when it gets there.
Skipping the lowering step is the classic PPM bug: the next move is silently ignored. A
handshake the drive never acknowledges returns `std::errc::timed_out`.

### Waiting for the end of the move

```cpp
motor.SetControl(epos4::controls::ProfilePosition{}.WithPosition(target));

// Target reached (Statusword bit 10) may still be set from the previous move:
// wait for it to drop, then for it to come back.
for (int i = 0; i < 20 && motor.IsTargetReached().GetValueRefreshed(); ++i) {
  std::this_thread::sleep_for(20ms);
}
while (!motor.IsTargetReached().GetValueRefreshed()) {
  std::this_thread::sleep_for(10ms);
}
```

«Target reached» means the actual position is within the **position window** of the target
for the **position window time** (`LimitConfigs::positionWindow`, `positionWindowTime`).
Check `HasFollowingError()` and `IsFaulted()` in the same loop to stop waiting on an axis
that will never arrive. The [profile position example](../examples/profile-position.md) has
the complete loop.

## Profile Velocity

The drive ramps to a velocity and holds it.

```cpp
struct ProfileVelocity
{
  VelocitySetpoint velocity{0};               // 0x60FF: rpm, or an angular velocity
  std::optional<std::uint32_t> acceleration;  // 0x6083: rpm/s
  std::optional<std::uint32_t> deceleration;  // 0x6084: rpm/s
};
```

```cpp
// 1000 rpm, reached in 1000 / 2000 = 0.5 s.
motor.SetControl(epos4::controls::ProfileVelocity{}
                   .WithVelocity(1000)
                   .WithAcceleration(2000)
                   .WithDeceleration(2000));

// Back to standstill on the deceleration ramp.
motor.SetControl(epos4::controls::ProfileVelocity{}.WithVelocity(0));
while (!motor.IsAtZeroSpeed().GetValueRefreshed()) {
  std::this_thread::sleep_for(10ms);
}
```

There is no handshake: the target velocity takes effect as soon as it arrives. The ramp
time is velocity ÷ acceleration. Under PVM, Statusword bit 12 means *speed is zero*
(`IsAtZeroSpeed()`) and bit 10 means the target velocity is reached (`IsTargetReached()`).

!!! note
    `epos4_sim` does not model Profile Velocity Mode. Use CSV in simulation, or test PVM on
    hardware.

## Profiled or cyclic?

| | Profiled (PPM, PVM) | Cyclic (CSP, CSV, CST) |
|---|---|---|
| Who generates the trajectory | the drive | your program |
| Commands | one per move | one per SYNC period |
| Calls | `SetControl()`, blocking | `StageTarget*()`, lock-free |
| Multi-axis coordination | each axis on its own | all axes on the same SYNC |
| Good for | point-to-point moves, simple velocity control | trajectory following, wheels, torque control |

`SetControl()` with a cyclic request works too - it writes the target once, with optional
offsets - but it checks the mode with an SDO read on every call. For a loop, use the
[cyclic path](cyclic-control.md).
