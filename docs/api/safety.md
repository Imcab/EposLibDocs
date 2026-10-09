# Brake, stops and protection

How a drive comes to rest decides whether a fault is an inconvenience or a dropped payload.
None of it is decided in code at the moment of stopping: it is configured on the drive
beforehand.

## Stop options

`StopOptionConfigs` sets what the drive does in each situation:

| Field | Object | When | Values | Device default |
|---|---|---|---|---|
| `shutdown` | `0x605B` | Operation enabled → Ready to switch on / Switch on disabled (`Disable()`) | `kDisableDrive` (cut power), `kSlowDownOnSlowDownRamp` | 0, cut power |
| `disableOperation` | `0x605C` | Operation enabled → Switched on | `kDisableDrive`, `kSlowDownOnSlowDownRamp` | 1, ramp down |
| `faultReaction` | `0x605E` | a fault with reaction "f" | `kDisableDrive`, `kSlowDownOnSlowDownRamp`, `kSlowDownOnQuickStopRamp` | 2, quick stop ramp |
| `abortConnectionOption` | `0x6007` | a communication fault ("a"): heartbeat lost, CAN passive | `kDisableVoltage`, `kSlowDownOnQuickStopRamp` | 3, quick stop ramp |
| `quickStop` | `0x605A` | `QuickStop()` | `kSlowDownOnQuickStopRampAndStayInQuickStop` (the only value the EPOS4 accepts) | 6 |
| `halt` | `0x605D` | `Halt()`, firmware ≥ `0x0180` | `kSlowDownRamp`, `kQuickStopRamp` | 1 |

The ramps are `MotionProfileConfigs::profileDeceleration` (slow down) and
`quickStopDeceleration` (quick stop).

```cpp
epos4::configs::StopOptionConfigs stops;
stops.shutdown = epos4::signals::ShutdownOption::kSlowDownOnSlowDownRamp;   // ramp, then cut
stops.faultReaction = epos4::signals::FaultReactionOption::kSlowDownOnQuickStopRamp;

epos4::configs::MotionProfileConfigs profile;
profile.quickStopDeceleration = 10000;   // rpm/s

motor.GetConfigurator().Apply(stops);
motor.GetConfigurator().Apply(profile);
motor.GetConfigurator().Save();
```

Errors marked **"d"** - *a secure movement is no longer possible* - always disable the drive
immediately, whatever is configured. The [Device error codes](../reference/error-codes.md)
mark each code's reaction.

## Holding brake

A holding brake keeps an axis from drifting at standstill and holds a load when power goes
away. It is **not** for stopping a moving load - the controller does that.

Three things are needed, and missing any one of them means the brake does nothing useful:

1. **The timings**, from the brake's data sheet:
   ```cpp
   epos4::configs::HoldingBrakeConfigs brake;
   brake.couplingTimeMs = 30;   // power off → full holding torque ("reaction time closing")
   brake.openingTimeMs = 40;    // power on → released ("reaction time opening")
   ```
   The drive waits these out: after removing power before considering the axis held, and
   after enabling before moving.
2. **An output** carrying the brake function:
   ```cpp
   epos4::configs::DigitalOutputConfigs outputs;
   outputs.output2 = epos4::signals::DigitalOutputFunction::kHoldingBrake;
   ```
3. **Standstill detection**, so the brake never clamps a turning shaft:
   ```cpp
   epos4::configs::StandstillConfigs standstill;
   standstill.window = 30;          // velocity units around zero
   standstill.windowTimeMs = 2;     // inside the window this long
   standstill.windowTimeoutMs = 1000;
   ```

The holding brake function also needs a main sensor configured: without feedback the drive
cannot tell standstill from slow motion.

The drive sequences the brake around the state machine by itself. Its state is readable,
not writable:

```cpp
auto brake = motor.GetBrakeState().Refresh().GetValue();   // kActive = clamped
```

There is deliberately no `ReleaseBrake()`: releasing a brake out of band on a loaded joint
drops the load. `DigitalOutputFunction::kSetBrakeGpio` exists for direct control, with no
timing and no interlock, as an explicit opt-in.

`openingVoltageDeciVolt` and `retainingVoltageDeciVolt` exist only on the Disk 60/8,
Disk 60/12 and Module/Compact 60/20.

## Communication loss

With `heartbeat_consumer: true` in the network description, each drive watches the master's
heartbeat and applies `abortConnectionOption` when it stops - by default decelerating on the
quick stop ramp, then disabling. This is what stops the drives when the control program
crashes or the cable comes loose. See [SYNC and heartbeat](../network/sync-heartbeat.md).

`CommunicationConfigs::communicationErrorBehavior` (`0x1029`) decides what the drive's NMT
state does at the same time: enter pre-operational (default) or stay.

## Supply and temperature

```cpp
epos4::configs::ProtectionConfigs protection;
protection.undervoltageLimitMv = 21000;          // a 24 V pack's cut-off
protection.overvoltageLimitMv = 30000;
protection.maxPowerStageTemperatureDeciC = 800;  // 80.0 °C
motor.GetConfigurator().Apply(protection);
```

On a battery, raising the undervoltage limit to the pack's cut-off makes the drive fault
cleanly (`0x3220`) before the battery's own protection drops the whole bus. The
undervoltage limit is written first, which is valid from any starting point.

The temperature limit is **not kept by `Save()`**: apply it on every boot. Watch the
approach with `GetPowerStageTemperature()` against `GetPowerStageTemperatureLimit()`.

## Limits

`LimitConfigs` - software position limits, maximum motor speed, following error window -
are what keep a mechanical problem from becoming a broken mechanism. See
[Configuration](configuration.md#limits).

## Safe Torque Off

On the Module and Compact variants, the STO inputs remove torque in hardware, independently
of the firmware. EposLib only reports them:

```cpp
auto sto = motor.GetStoInputs().Refresh().GetValue();     // input1Active, input2Active
auto card = motor.GetStoCardStatus().Refresh().GetValue(); // 60/20 STO card only
```

What each input state means for the power stage is in the *EPOS4 Application Notes*; the
library does not guess a "torque allowed" flag from them.
