# Units

## What the drive speaks

The EPOS4's units are almost entirely fixed. «SI unit position» (`0x60A8`) and «SI unit
acceleration» (`0x60AA`) each have exactly one legal value, so whatever is configured:

| Quantity | Drive unit | Objects |
|---|---|---|
| Position | **quadcounts** (increments) of the motor encoder | `0x6064`, `0x607A`, `0x60F4` |
| Velocity | **rpm** of the motor (prefix configurable, `0x60A9`) | `0x606C`, `0x60FF`, `0x6081` |
| Acceleration | **rpm/s** | `0x6083`, `0x6084`, `0x6085` |
| Torque | **thousandths of «Motor rated torque»** (`0x6076`) | `0x6071`, `0x6077`, `0x60B2` |
| Current | mA | `0x30D1`, `0x3001` |
| Supply voltage | 0.1 V | `0x2200:01` |
| Temperature | 0.1 °C | `0x3201` |

EposLib keeps the motion signals in these raw units - they are what the manual, EPOS Studio
and every tuning note use - and converts the ones whose raw unit means nothing on its own:
current, voltage and temperature come back as `units::current::ampere_t`,
`units::voltage::volt_t` and `units::temperature::celsius_t`.

### Quadcounts

An incremental encoder sold as **500 CPR** produces 500 pulses per turn on each channel. The
drive counts every edge of both channels, so it sees **2000 quadcounts** per turn:

```text
4 × pulses/rev = increments/rev = quadcounts/rev
```

`Encoder::QuadCountsPerRevolution(500)` returns 2000. Getting this factor wrong scales every
move by four.

### Thousandths of rated torque

The drive computes **«Motor rated torque»** from the motor data - nominal current × torque
constant - and expresses every torque as thousandths of it, at the **motor shaft**:

```text
rated torque = 9.28 A × 104.79 mN·m/A = 0.9724 N·m
100 ‰ = 0.0972 N·m        1000 ‰ = 0.9724 N·m
```

If the motor data was never configured, the rated torque is 0 and a torque in N m has no
meaning. Every conversion in the library refuses rather than guess - see
[Cyclic control](../api/cyclic-control.md#torque).

## Working in real units

Tell the device the encoder resolution and the gear ratio once:

```cpp
// 500-pulse encoder = 2000 quadcounts per motor turn; 1:100 gearbox.
motor.SetMechanism(2000, 1.0 / 100.0);
```

- The first argument is **quadcounts per motor turn**.
- The second is **output turns per motor turn**: a 1:100 reduction is `1.0 / 100.0`. With it,
  every angle and velocity is at the **output** - the joint or the wheel.
- `GetEncoder().SetMechanismFromDevice(gearRatio)` reads the resolution the drive computed
  (`0x3000:05`) instead of repeating it in code.

Then setpoints take either form:

```cpp
using namespace units::literals;

motor.SetControl(epos4::controls::ProfilePosition{}.WithPosition(50000));    // quadcounts
motor.SetControl(epos4::controls::ProfilePosition{}.WithPosition(90_deg));   // at the output
motor.StageTargetVelocity(1500);       // rpm at the motor
motor.StageTargetVelocity(15_rpm);     // rpm at the output
motor.StageTargetTorque(std::int16_t{250});   // thousandths of rated torque
motor.StageTargetTorque(0.5_Nm);       // N m at the motor shaft
```

A plain number is always the raw drive unit; a quantity is converted. A quantity used before
`SetMechanism()` is **rejected** - `SetControl()` returns `std::errc::invalid_argument` and
`StageTarget*()` returns false - rather than resolved against a guessed resolution.

!!! warning "Integers are raw, always"
    `WithPosition(90)` is 90 quadcounts, not 90 degrees. Use the literal `90_deg` (or
    `units::angle::degree_t{90.0}`) for an angle.

The units are [nholthaus/units](https://github.com/nholthaus/units), the same library
WPILib's C++ API uses, shipped as the `ros2units` package. Any convertible unit is accepted:
`units::angle::radian_t`, `units::angle::turn_t` and `units::angle::degree_t` all work for a
position.

## Converting feedback

```cpp
const auto & mechanism = motor.GetMechanism();
units::angle::degree_t angle = mechanism.ToAngle(motor.GetCachedPosition());
auto speed = mechanism.ToAngularVelocity(motor.GetCachedVelocity());   // rpm at the output

auto & encoder = motor.GetEncoder();
std::optional<units::angle::turn_t> a = encoder.GetAngle();   // nullopt without a mechanism

auto torque = motor.PerThousandToTorque(motor.GetCachedTorque());   // optional<newton_meter_t>
```

`MechanismScale` - what `SetMechanism()` builds - has the conversions both ways:

| | |
|---|---|
| `ToAngle(quadCounts)` | → `turn_t` at the output |
| `ToQuadCounts(angle)` | → quadcounts, rounded to nearest (truncating would make a round trip walk) |
| `ToAngularVelocity(rpm)` | → rpm at the output |
| `ToRpm(velocity)` | → rpm at the motor |
| `ToRpmPerSecondAtOutput(rpmPerSecond)` / `FromRpmPerSecondAtOutput(x)` | accelerations |

And free functions in `core/UnitConversion.hpp`: `ToTorque()` / `ToPerThousand()` (with the
rated torque in µN m), `ToCurrent()` / `ToMilliamps()`, `ToVoltage()`, `ToTemperature()`.
