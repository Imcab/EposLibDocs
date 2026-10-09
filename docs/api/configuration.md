# Configuration

Configuration is everything that is set **before** commanding: motor data, controller gains,
limits, the motion profile, input mappings, stop behaviour. It goes over SDO and is slow
compared to commanding, so it lives apart, on the **configurator**:

```cpp
epos4::Configurator & configurator = motor.GetConfigurator();
```

## Only what you set is written

Every field of every configuration group is a `std::optional`. `Apply()` writes the fields
that are set and **leaves everything else on the drive untouched**:

```cpp
epos4::configs::Epos4Configuration cfg;
cfg.limits.maxMotorSpeed = 4000;
cfg.velocityControl.p = 150000;
cfg.velocityControl.i = 20000;

motor.GetConfigurator().Apply(cfg);   // three writes - nothing else changes
```

A configuration struct with defaults would silently overwrite gains and motor data that
someone spent a day tuning the first time it was applied half-filled. Here an unset field
means *do not touch*, never *reset to zero*.

## Apply, Refresh, Save

```cpp
std::error_code Apply(const configs::Epos4Configuration & config);
std::error_code Apply(const configs::<Group> & config);       // one group
std::error_code Refresh(configs::Epos4Configuration & config);
std::error_code Refresh(configs::<Group> & config);
std::error_code Save();
std::error_code RestoreDefaults();
```

**`Apply()`** validates first - input mappings, the dual loop filter - and writes nothing if
validation fails. It then writes the set fields in order and stops at the first write the
drive refuses, returning its error (an [SDO abort code](../reference/sdo-abort-codes.md)).

**`Refresh()`** reads every field of the group from the drive - every field `Apply()` could
write, plus the read-only values the drive computes. It keeps going past an object it cannot
read and returns the first error; such fields stay unset. Objects only some hardware or
firmware has are simply left unset when absent.

**`Save()`** stores the drive's current parameters in non-volatile memory (`0x1010`).
**Without it, everything applied is lost at the next power cycle.**

**`RestoreDefaults()`** restores the factory defaults (`0x1011`), effective after a reset.
Only with the motor unpowered, and in NMT pre-operational - the drive refuses it in
Operational.

!!! danger "RestoreDefaults() resets everything"
    Factory defaults include the motor data and every tuned gain. To fix one setting, apply
    that setting; do not restore defaults.

### Read, modify, write

```cpp
auto & configurator = motor.GetConfigurator();

epos4::configs::VelocityControlConfigs gains;
configurator.Refresh(gains);                 // what the drive has
gains.p = *gains.p * 12 / 10;                // +20 %
configurator.Apply(gains);                   // writes every field Refresh() filled
configurator.Save();
```

After a `Refresh()` every readable field is set, so applying the same struct writes them all
back - fine for one group, but think twice before doing it with a whole
`Epos4Configuration`.

## «Power Disable» objects

The manual only allows some objects to be written with **no power on the motor**: the axis
configuration (`0x3000`), the number of pole pairs, the gear (except its maximum input
speed), the SI units and every encoder object. An `Apply()` that contains any of them, while
the drive is in a powered state, returns `std::errc::operation_not_permitted` **before the
first write** instead of being aborted by the drive halfway through:

```cpp
motor.Disable();                              // «Power Disable»
configurator.Apply(motorAndEncoderConfig);
```

`configs::RequiresPowerDisabled(entry)` and `signals::IsPowerDisabled(state)` expose the rule.

## The groups

`Epos4Configuration` aggregates every group. Each also has its own `Apply()` and `Refresh()`.

| Member | Group | Objects | |
|---|---|---|---|
| `motor` | `MotorConfigs` | `0x6402`, `0x3001`, `0x3002`, `0x6076` | motor type, nominal current, current limit, pole pairs, thermal time constant, torque constant, R, L; rated torque (read-only) |
| `gear` | `GearConfigs` | `0x3003` | reduction, max input speed, inverted direction |
| `axis` | `AxisConfigs` | `0x3000` | sensors, control structure, commutation sensors |
| `currentControl` | `CurrentControlConfigs` | `0x30A0` | P, I |
| `positionControl` | `PositionControlConfigs` | `0x30A1` | P, I, D, feed-forward, I-gain unit |
| `velocityControl` | `VelocityControlConfigs` | `0x30A2` | P, I, feed-forward, filter cut-off |
| `velocityObserver` | `VelocityObserverConfigs` | `0x30A3` | observer gains, friction, inertia |
| `dualLoop` | `DualLoopConfigs` | `0x30AE` | dual loop position control |
| `motionProfile` | `MotionProfileConfigs` | `0x6081`-`0x6086`, `0x607F`, `0x60C5` | PPM/PVM profile, quick stop deceleration |
| `cyclic` | `CyclicConfigs` | `0x60C2` | interpolation time period |
| `siUnits` | `SiUnitConfigs` | `0x60A9` | velocity prefix |
| `limits` | `LimitConfigs` | `0x607D`, `0x6080`, `0x6065`-`0x6068` | software position limits, max speed, following error, position window |
| `homing` | `HomingConfigs` | `0x6098`-`0x609A`, `0x30B0`-`0x30B2` | see [Homing](homing.md) |
| `stopOptions` | `StopOptionConfigs` | `0x605A`-`0x605E`, `0x6007` | see [Brake, stops and protection](safety.md) |
| `holdingBrake` | `HoldingBrakeConfigs` | `0x3158` | brake timing and voltages |
| `standstill` | `StandstillConfigs` | `0x30E0` | standstill window |
| `digitalInputs` | `DigitalInputConfigs` | `0x3141`, `0x3142` | input functions, polarity |
| `digitalOutputs` | `DigitalOutputConfigs` | `0x3150`, `0x3151` | output functions, polarity |
| `analogInputs` | `AnalogInputConfigs` | `0x3161`-`0x3171` | functions, calibration, set-value scaling |
| `analogOutputs` | `AnalogOutputConfigs` | `0x3181` | output functions |
| `protection` | `ProtectionConfigs` | `0x2201`, `0x3201:04` | supply voltage limits, temperature limit |
| `customMemory` | `CustomPersistentMemoryConfigs` | `0x210C` | four words for the application |
| `communication` | `CommunicationConfigs` | `0x1016`, `0x1017`, `0x1029`, `0x2000`-`0x2006` | heartbeat, node-ID, bit rates |

Every field, with its object, type and unit: [Configuration fields](../reference/configuration-fields.md).
The encoder groups are applied through the [encoder subsystem](encoders.md).

## Common recipes

### Motor data

Normally entered once with EPOS Studio's startup wizard, from the motor's data sheet. From
code:

```cpp
epos4::configs::MotorConfigs motorData;
motorData.motorType = epos4::signals::MotorType::kBrushlessSinusoidal;
motorData.nominalCurrent = 9280;        // mA
motorData.outputCurrentLimit = 9280;    // mA
motorData.numberOfPolePairs = 7;        // «Power Disable» only
motorData.torqueConstant = 104790;      // µN m/A
motorData.thermalTimeConstant = 400;    // 0.1 s

motor.Disable();
motor.GetConfigurator().Apply(motorData);
motor.GetConfigurator().Save();
```

The drive then computes the rated torque: `motorData.ratedTorque` after a `Refresh()`.

### Limits

```cpp
epos4::configs::LimitConfigs limits;
limits.maxMotorSpeed = 5000;          // rpm
limits.minPositionLimit = -200000;    // quadcounts
limits.maxPositionLimit = 200000;
limits.followingErrorWindow = 2000;   // quadcounts before fault 0x8611
limits.positionWindow = 20;           // "target reached" within 20 qc...
limits.positionWindowTime = 10;       // ...for 10 ms
motor.GetConfigurator().Apply(limits);
```

### Making a setting survive power cycles

Either `Save()` it on the drive, or put it in the network description so the master writes
it on every boot (`sdo:` in [bus.yml](../network/bus-yml.md#sdo-objects-written-at-boot)).
Saving suits what belongs to the drive (motor data, gains); the network description suits
what belongs to the network (interpolation period, heartbeat).

!!! warning "Node-ID and bit rate"
    `CommunicationConfigs::nodeId` and `canBitRate` take effect only after `Save()` and a
    power cycle, and they are written last for that reason. A drive moved to a node-ID the
    master's DCF does not describe is no longer booted. And its saved PDO COB-IDs still
    belong to the old node-ID - see [Boot and PDOs](../troubleshooting/boot-and-pdo.md#cob-ids).

!!! note "Not kept by Save()"
    The power stage temperature limit (`ProtectionConfigs::maxPowerStageTemperatureDeciC`,
    `0x3201:04`) is not stored by `Save()`: apply it on every boot.
