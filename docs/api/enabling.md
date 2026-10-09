# Enabling and stopping

Nothing moves until the drive is in **Operation enabled**. These calls walk the
[CiA 402 state machine](../concepts/state-machine.md) for you.

## Enable

```cpp
bool Enable(std::chrono::milliseconds timeout = 1000ms);
```

Walks the drive to Operation enabled: Shutdown, Switch on, Enable operation - one transition
per round trip, retrying through transient bus errors until the timeout. Returns:

- `true` when the drive reports Operation enabled;
- `false` on timeout, when the bus is not running, or **when the drive is in Fault** -
  faults are never cleared implicitly.

```cpp
if (!motor.Enable()) {
  if (motor.IsFaulted()) {
    std::printf("%s\n", motor.DescribeLastError().c_str());
  } else {
    std::printf("timed out in %s\n",
      epos4::signals::ToString(motor.GetState().Refresh().GetValue()));
  }
}
```

If the drive has a holding brake configured, it releases it during the transition, waiting
the brake's opening time; see [Brake, stops and protection](safety.md).

## Disable

```cpp
bool Disable(std::chrono::milliseconds timeout = 1000ms);
```

Sends *Disable voltage* and waits for Switch on disabled. Completes even on a faulted axis:
in Fault no power reaches the motor, which is all `Disable()` promises.

How the motor comes to rest is the drive's decision, set by its stop options - by default
the EPOS4 cuts the power stage and the motor coasts. A transition out of Operation enabled
also waits for **standstill** (`StandstillConfigs`), which is why `Disable()` can take longer
than three cycles.

!!! warning "Disabling a loaded axis"
    Without power the motor holds nothing. On a joint carrying a load against gravity,
    `Disable()` without a holding brake drops the load. Bring the axis to a supported
    position first, or configure the brake.

## Clear a fault

```cpp
bool ClearFault(std::chrono::milliseconds timeout = 3000ms);
```

1. Returns `true` at once if the drive is not in Fault.
2. Reads the error code. For the faults whose recovery starts with an **NMT reset
   communication** - `0x8130` heartbeat lost and `0x8120` CAN passive - sends it and waits
   until the master has booted the node again.
3. Sends the fault reset: a rising edge on Controlword bit 7.
4. Returns `true` when the drive leaves Fault, `false` if it stays (the cause is still
   present) or the timeout passes.

After a successful `ClearFault()` the drive is in Switch on disabled: call `Enable()` again.

```cpp
if (motor.IsFaulted()) {
  const auto code = motor.GetErrorCode().Refresh().GetValue();
  if (epos4::signals::ClearsPosition(code)) {
    needsHoming = true;           // the reset will clear the position
  }
  if (motor.ClearFault()) {
    motor.Enable();
  }
}
```

## State queries

```cpp
bool IsEnabled();    // Operation enabled
bool IsFaulted();    // Fault or Fault reaction active
```

Both refresh the state signal, so they cost a read unless PDOs are active. A failed read
reports `false` for both.

## Halt

```cpp
std::error_code Halt();
// or
motor.SetControl(epos4::controls::Halt{});
```

Sets Controlword bit 8: the profile modes (PPM, PVM, Homing) decelerate and the drive
**stays in Operation enabled**. With firmware `0x0180` or later, *how* it decelerates is the
halt option code (`StopOptionConfigs::halt`): the profile deceleration (default) or the
quick stop deceleration.

!!! bug "Known issue: the halt bit stays set"
    In EposLib 0.1, nothing lowers Controlword bit 8 after `Halt()`: the profile requests
    only touch bits 4-6. Until the library is fixed, a Profile Position or Profile Velocity
    command, or a homing run, issued after `Halt()` stays halted - verified with
    `epos4_sim`, where a `Home()` after `Halt()` never starts. To stop and continue, use
    `QuickStop()` and then `Enable()` instead, which do not involve bit 8.

## Quick stop

```cpp
std::error_code QuickStop();
```

Drives Controlword bit 2 low: the drive decelerates on the **quick stop deceleration**
(`MotionProfileConfigs::quickStopDeceleration`, `0x6085`) and ends in **Quick stop active**.
`Enable()` brings it back to Operation enabled. On the EPOS4 the quick stop option code has a
single legal value: decelerate and stay in Quick stop active.

Both `Halt()` and `QuickStop()` work while the cyclic path is active: the Controlword they
write is also the one published on every SYNC from then on.

## Which one

| Situation | Call |
|---|---|
| Pause a profile move (see the known issue above) | `Halt()` |
| Stop now, controlled, keep power | `QuickStop()` |
| Done, remove power | `Disable()` |
| Something went wrong and the drive faulted | read `DescribeLastError()`, then `ClearFault()` |
