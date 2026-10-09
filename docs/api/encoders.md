# Encoders

Feedback has its own subsystem, reached through the device:

```cpp
epos4::Encoder & encoder = motor.GetEncoder();   // #include "epos4/hardware/Encoder.hpp"
```

It is a view onto the same drive, not a separate device: it holds a reference to the
`Epos4` and must not outlive it.

## Sensor slots

The EPOS4 has three sensor slots, configured in `0x3000:01`. The slot constrains the type:

| Slot | Possible sensors | Enum |
|---|---|---|
| Sensor 1 | none, digital incremental encoder 1 | `Sensor1Type` |
| Sensor 2 | none, digital incremental encoder 2, analog incremental SinCos, SSI absolute | `Sensor2Type` |
| Sensor 3 | none, digital Hall sensors (EC motors only) | `Sensor3Type` |

```cpp
epos4::configs::SensorsConfigs sensors;
encoder.Refresh(sensors);
// sensors.sensor1 == Sensor1Type::kDigitalIncrementalEncoder1
// sensors.sensor3 == Sensor3Type::kDigitalHallSensor
// sensors.mainSensorResolution == 2048 (quadcounts per revolution, computed by the drive)
```

A typical maxon EC motor with an encoder: sensor 1 = incremental encoder, sensor 3 = Hall
sensors for commutation. A second encoder on the output shaft goes in slot 2 - and disables
the high-speed digital inputs, which share its pins.

## Configuring

Every `Encoder::Apply()` needs the motor **unpowered** - the manual allows writing the encoder
objects only in «Power Disable» - and returns `std::errc::operation_not_permitted` otherwise,
before writing anything.

```cpp
motor.Disable();

epos4::configs::SensorsConfigs sensors;
sensors.sensor1 = epos4::configs::Sensor1Type::kDigitalIncrementalEncoder1;
sensors.sensor2 = epos4::configs::Sensor2Type::kNone;
sensors.sensor3 = epos4::configs::Sensor3Type::kDigitalHallSensor;
encoder.Apply(sensors);

epos4::configs::DigitalIncrementalEncoderConfigs incremental;
incremental.encoderNumber = 1;
incremental.pulsesPerRevolution = 512;            // pulses, not quadcounts
epos4::configs::IncrementalEncoderType type;
type.index = epos4::configs::IndexType::kWithIndex;
type.direction = epos4::configs::EncoderDirection::kMaxon;
incremental.type = type;
encoder.Apply(incremental);

motor.GetConfigurator().Save();
```

!!! danger "Changing the sensors clears the position"
    The manual: «Position referenced to home position», Position actual value and the
    additional position values are cleared when the sensor configuration changes. An axis
    that was homed must be homed again afterwards.

### The encoder types

**Digital incremental** (`DigitalIncrementalEncoderConfigs`, `0x3010` / `0x3020` - pick with
`encoderNumber`):

| Field | |
|---|---|
| `pulsesPerRevolution` | 16 to 2 500 000 **pulses** per turn. 500 CPR = 500 here = 2000 quadcounts. |
| `type.index` | `kNoIndex` (2 channels), `kWithIndex` (3 channels), `kWithIndexNoSupervision` |
| `type.direction` | `kMaxon` or `kInverted` (also: encoder mounted on the motor shaft's other end) |
| `type.method` | speed measurement: `kTimeBetweenEdges` (low speed) or `kEdgesPerControlCycle` (high speed) |
| `indexPosition` | read-only: where the index was last seen |

**Analog incremental SinCos** (`AnalogIncrementalEncoderConfigs`, `0x3011`): `periodsPerTurn`
and `interpolationBits` (resolution = periods × 2^bits, 64 to 10 000 000 inc/rev), `type`
(index, direction). Setting only one of the two resolution fields takes the other from the
drive's default (2048 periods, 4 bits).

**SSI absolute** (`SsiAbsoluteEncoderConfigs`, `0x3012`): data rate, the frame layout
(`specialBitsLeading`, `multiTurnBits`, `singleTurnBits`, `specialBitsTrailing`, at most 62
bits together), encoding (`kGray` / `kBinary`), timeouts, commutation offset for third-party
encoders on EC motors, and which bits form the position (`positionMultiTurnBits`,
`positionSingleTurnBits`). `refreshFrequency` is read-only.

**Hall sensors** (`HallSensorConfigs`, `0x301A`): `type.polarity` and `type.method`. Note the
bit layout differs from the incremental encoder's - the library encodes each type word
separately for that reason.

All fields: [Configuration fields](../reference/configuration-fields.md#sensorsconfigs).

### Reading back

Every group has a `Refresh()`, allowed in any power state. For the digital encoder, set
`encoderNumber` first - it picks the object:

```cpp
epos4::configs::DigitalIncrementalEncoderConfigs enc;
enc.encoderNumber = 1;
encoder.Refresh(enc);
std::printf("%u pulses/rev = %u qc/rev\n", *enc.pulsesPerRevolution,
  epos4::Encoder::QuadCountsPerRevolution(*enc.pulsesPerRevolution));
```

## Angles at the output

```cpp
void SetMechanism(std::uint32_t quadCountsPerRevolution, double gearRatio = 1.0);
std::error_code SetMechanismFromDevice(double gearRatio = 1.0);
std::optional<units::angle::turn_t> GetAngle();
std::optional<units::angular_velocity::revolutions_per_minute_t> GetAngularVelocity();
```

`Epos4::SetMechanism()` is a shortcut to the encoder's; the scale is shared.
`SetMechanismFromDevice()` reads the resolution the drive computed (`0x3000:05`) so it does
not have to be repeated in code - only the gear ratio is yours. It fails if the drive
reports 0; fall back to computing it from the encoder's pulses:

```cpp
if (encoder.SetMechanismFromDevice(1.0 / 100.0)) {
  encoder.SetMechanism(epos4::Encoder::QuadCountsPerRevolution(512), 1.0 / 100.0);
}
if (auto angle = encoder.GetAngle()) {
  units::angle::degree_t deg = *angle;
}
```

`GetAngle()` and `GetAngularVelocity()` refresh position and velocity and convert; they
return `nullopt` when the mechanism was never set, or the read failed - a joint angle
silently out by the gear ratio is worse than no reading.

## Readings

| Getter | Type | Object | |
|---|---|---|---|
| `GetPosition()` | `std::int32_t` | `0x6064` | the same signal as the device's |
| `GetVelocity()` | `std::int32_t` | `0x606C` | |
| `GetIndexPosition(encoderNumber)` | `std::int32_t` | `0x3010:04` / `0x3020:04` | where the index pulse was last seen |
| `GetHallPattern()` | `std::uint16_t` | `0x301A:02` | live Hall state: an impossible pattern is fault `0x7388` |
| `GetSsiRawPosition()` | `std::uint32_t` | `0x3012:09` | the raw frame, to check the bit layout |
| `GetMainSensorResolution()` | `std::uint32_t` | `0x3000:05` | quadcounts per revolution |
| `GetSensorPosition(slot)` | `std::int32_t` | `0x60E4` | each sensor on its own |
| `GetSensorVelocity(slot)` | `std::int32_t` | `0x60E5` | |
| `GetSensorVelocityAveraged(slot)` | `std::int32_t` | | 5 Hz low-pass |

`GetSensorPosition(SensorSlot::kSensor2)` is how to compare a motor encoder with a second one
on the output shaft. A slot with no sensor reads 0.

## Unit helpers

```cpp
static constexpr std::uint32_t QuadCountsPerRevolution(std::uint32_t pulsesPerRevolution);  // × 4
static constexpr std::uint32_t SinCosResolution(std::uint32_t periodsPerTurn, std::uint8_t interpolationBits);
```

The complete program: [encoder setup](../examples/encoder-setup.md).
