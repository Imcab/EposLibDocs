# Status signals

A **status signal** is one value read from the drive, together with what is needed to decide
whether to trust it: the error of the last read and when it was taken. Every getter on
`Epos4` that returns a measurement returns a `signals::StatusSignal<T>&`.

## Refresh explicitly

```cpp
auto & position = motor.GetPosition();   // a reference to the device's signal

position.Refresh();                      // go to the bus (or to the last PDO)
if (!position.GetStatus()) {             // falsy error_code = good read
  std::int32_t qc = position.GetValue(); // the value from that refresh
}

// The same in one expression:
std::int32_t qc = motor.GetPosition().Refresh().GetValue();
std::int32_t qc2 = motor.GetPosition().GetValueRefreshed();
```

A getter never talks to the bus by itself: `GetValue()` returns the last refreshed value.
Reading is explicit because an SDO round trip is two CAN frames, and a getter that quietly
did one on every call would be a bandwidth problem hidden in an innocent accessor. When the
value travels in a PDO, `Refresh()` costs no bus traffic, and the calling code does not
change.

## The StatusSignal API

| Member | |
|---|---|
| `Refresh()` | read now; returns `*this` for chaining |
| `GetValue()` | the last successfully read value (no bus access) |
| `GetValueRefreshed()` | `Refresh().GetValue()` |
| `GetStatus()` | `std::error_code` of the last refresh; falsy when good |
| `GetTimestamp()` | `steady_clock` time of the last successful read |
| `GetAge()` | how long ago that was |
| `HasValue()` | whether any read ever succeeded |
| `IsNear(target, tolerance)` | the cached value is within tolerance of target (inclusive) |

A failed refresh keeps the previous value and timestamp, and sets the status. So check
`GetStatus()` before acting on `GetValue()`, or check `GetAge()` - a position several seconds
old is not a position.

```cpp
if (motor.GetPosition().Refresh().IsNear(target, 50)) {   // within 50 quadcounts
  // arrived
}
```

`IsNear()` compares the cached value and ignores the status: combine it with a status check
where a failed read must not count as "near".

## Several at once

```cpp
std::error_code RefreshAll(Signals & ... signals);
bool IsAllGood(const Signals & ... signals);
```

```cpp
auto & voltage = motor.GetSupplyVoltage();
auto & temperature = motor.GetPowerStageTemperature();
auto & current = motor.GetMotorCurrentAveraged();

epos4::signals::RefreshAll(voltage, temperature, current);   // concurrently
if (epos4::signals::IsAllGood(voltage, temperature, current)) {
  // every one has a value and its last refresh succeeded
}
```

`RefreshAll()` refreshes every signal on its own thread and waits for all of them; it
returns the first error, in argument order. Signals of **different drives** are read in
parallel - each drive has its own SDO channel - so the voltage of six axes costs about one
round trip. Signals of the same drive queue on its channel. It starts a thread per signal:
fine at the few hertz diagnostics run at, far too much for a control loop, which has the
[lock-free accessors](cyclic-control.md#reading-feedback).

!!! warning "One thread per signal"
    A signal is not thread-safe on its own. Refresh a given signal from one thread at a
    time, and pass each signal to `RefreshAll()` once.

## Every signal

**PDO** marks the values carried by the example network's TPDOs: their refresh costs no bus
traffic while PDOs flow. Everything else is an SDO read.

### State

| Getter | Type | Object | |
|---|---|---|---|
| `GetState()` | `signals::State` | `0x6041` decoded | **PDO**; `protocol_error` for a pattern matching no state |
| `GetStatusword()` | `std::uint16_t` | `0x6041` | **PDO** |
| `GetOperationMode()` | `signals::OperationMode` | `0x6061` | the active mode |

### Statusword bits

Exposed by meaning, because bits 10, 12 and 13 mean different things in each mode. All
`StatusSignal<bool>`, read from the Statusword (**PDO**).

| Getter | Bit | Valid in |
|---|---|---|
| `IsTargetReached()` | 10 | PPM, PVM, HMM |
| `IsSetpointAcknowledged()` | 12 | PPM |
| `IsAtZeroSpeed()` | 12 | PVM |
| `IsHomingAttained()` | 12 | HMM |
| `IsFollowingCommand()` | 12 | CSP, CSV, CST - the drive follows the command value |
| `HasFollowingError()` | 13 | PPM, CSP |
| `HasHomingError()` | 13 | HMM |
| `IsInternalLimitActive()` | 11 | any - I2t, current or speed limiting |
| `HasWarning()` | 7 | any |
| `IsPositionReferenced()` | 15 | any - homed, and not lost since |
| `IsRemote()` | 9 | any - NMT Operational |
| `IsVoltageEnabled()` | 4 | any - power stage on |

### Motion

Raw drive units: quadcounts, rpm, thousandths of rated torque.

| Getter | Type | Object | |
|---|---|---|---|
| `GetPosition()` | `std::int32_t` | `0x6064` | **PDO**, quadcounts |
| `GetPositionDemand()` | `std::int32_t` | `0x6062` | the trajectory generator's output |
| `GetVelocity()` | `std::int32_t` | `0x606C` | **PDO**, rpm |
| `GetVelocityDemand()` | `std::int32_t` | `0x606B` | |
| `GetVelocityAveraged()` | `std::int32_t` | `0x30D3:01` | 5 Hz low-pass: for display and logs |
| `GetTorque()` | `std::int16_t` | `0x6077` | **PDO**, thousandths of rated torque |
| `GetTorqueAveraged()` | `std::int16_t` | `0x30D2:01` | 50 Hz low-pass |
| `GetFollowingError()` | `std::int32_t` | `0x60F4` | quadcounts |
| `GetCurrentDemand()` | `std::int16_t` | `0x30D0` | |
| `GetMotorRatedTorque()` | `std::uint32_t` | `0x6076` | µN m; the base of every torque value |

The encoder subsystem adds per-sensor position and velocity, index positions, Hall pattern
and SSI raw position: see [Encoders](encoders.md).

### Power and thermal

In physical units, because their raw units (mA, tenths of a volt, tenths of a degree) carry
no meaning of their own and only invite a factor-of-ten mistake.

| Getter | Type | Object | |
|---|---|---|---|
| `GetMotorCurrent()` | `units::current::ampere_t` | `0x30D1:02` | instantaneous; mostly PWM ripple |
| `GetMotorCurrentAveraged()` | `units::current::ampere_t` | `0x30D1:01` | 50 Hz low-pass: the one to log |
| `GetSupplyVoltage()` | `units::voltage::volt_t` | `0x2200:01` | the battery, on a rover |
| `GetPowerStageTemperature()` | `units::temperature::celsius_t` | `0x3201:01` | |
| `GetPowerStageTemperatureLimit()` | `units::temperature::celsius_t` | `0x3201:04` | above it: fault `0x4210` |
| `GetMotorI2tPercent()` | `std::uint16_t` | `0x3200:01` | above 100 the drive limits current |
| `GetPowerStageI2tPercent()` | `std::uint16_t` | `0x3200:02` | |
| `GetPwmDutyCyclePerMille()` | `std::uint16_t` | `0x3203:01` | near 1000: no voltage headroom left |
| `GetStoInputs()` | `signals::StoInputStates` | `0x3202:01` | Module/Compact only |
| `GetStoCardStatus()` | `signals::StoCardStatus` | `0x3202:02` | Module/Compact 60/20 only |

What these tell you before anything faults:

- **Supply voltage sagging** under load shows up here before an undervoltage fault (`0x3220`).
- **Temperature** approaching the limit is the warning before a thermal overload (`0x4210`).
- **I2t above 100 %** means the drive is already limiting current - the joint "feels weak"
  after a long hold.
- **PWM duty cycle near 1000** means the motor has run out of voltage: the speed you get at
  this supply is the limit, not the controller.

### Errors

| Getter | Type | Object | |
|---|---|---|---|
| `GetErrorCode()` | `std::uint16_t` | `0x603F` | the current fault; see [Diagnostics](diagnostics.md) |
| `GetErrorRegister()` | `std::uint8_t` | `0x1001` | the kind of error, as flags |

### Digital and analog I/O

| Getter | Type | Object | |
|---|---|---|---|
| `GetDigitalInputs()` | `std::uint32_t` | `0x60FD` | by function, after polarity |
| `GetDigitalInputPins()` | `std::uint16_t` | `0x3141:01` | by pin, before polarity: for wiring checks |
| `GetDigitalOutputs()` | `std::uint32_t` | `0x60FE:01` | by function |
| `GetDigitalOutputPins()` | `std::uint16_t` | `0x3150:01` | by pin, after polarity |
| `GetBrakeState()` | `signals::BrakeState` | `0x3158:03` | `kActive` = clamped |
| `GetAnalogInputVoltage(AnalogInput)` | `units::voltage::volt_t` | `0x3160:01/02` | |
| `GetAnalogInputGeneralPurpose(AnalogGeneralPurpose)` | `units::voltage::volt_t` | `0x3162:01/02` | |
| `GetAnalogOutputVoltage(AnalogOutput)` | `units::voltage::volt_t` | `0x3180:01/02` | |

### Communication and capabilities

| Getter | Type | Object | |
|---|---|---|---|
| `GetCanBitRate()` | `signals::CanBitRate` | `0x200A` | the rate in use |
| `GetActiveFieldbus()` | `signals::Fieldbus` | `0x2010` | |
| `GetSupportedDriveModes()` | `std::uint32_t` | `0x6502` | bit field; `SupportsMode()` decodes it |

## Not a signal

A few reads return directly instead of through a signal:

| Call | Returns |
|---|---|
| `IsEnabled()`, `IsFaulted()` | `bool`, refreshing the state |
| `IsInputActive(function)`, `IsOutputActive(function)` | `bool` |
| `IsNegativeLimitActive()`, `IsPositiveLimitActive()`, `IsHomeSwitchActive()` | `bool` |
| `SupportsMode(mode)` | `bool` |
| `GetSupportedHomingMethods(out)` | `std::error_code`, fills a vector |
| `GetIdentity(out)`, `GetErrorHistory(out)`, `GetTouchProbe(out)`, `ReadPdoMapping(out)` | `std::error_code` |
| `GetCached*()`, `IsCyclicHealthy()`, `GetBootStatus()` | lock-free or no bus access |
