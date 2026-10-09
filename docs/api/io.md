# Digital and analog I/O

## Digital inputs

Each physical input carries one **function**, mapped in `0x3142`:

| Function | Value | |
|---|---|---|
| `kNegativeLimitSwitch`, `kPositiveLimitSwitch` | 0, 1 | raises a limit error when hit outside homing |
| `kHomeSwitch` | 2 | for homing |
| `kGeneralPurposeA` ... `kGeneralPurposeH` | 16-23 | read by the application |
| `kNegativeLimitSwitchNoError`, `kPositiveLimitSwitchNoError` | 24, 25 | limit switches for homing that do not raise a limit error |
| `kTouchProbe` | 26 | latches the position on an edge |
| `kDriveEnable` | 27 | enables the drive, or clears a fault |
| `kQuickStop` | 28 | quick stop |
| `kNone` | 255 | |

Device defaults: input 1 negative limit, input 2 positive limit, input 3 home switch,
input 4 general purpose D, high-speed inputs none.

```cpp
using epos4::signals::DigitalInputFunction;

epos4::configs::DigitalInputConfigs inputs;
inputs.input1 = DigitalInputFunction::kNegativeLimitSwitch;
inputs.input2 = DigitalInputFunction::kPositiveLimitSwitch;
inputs.input4 = DigitalInputFunction::kGeneralPurposeA;
inputs.polarity = 0x0000;          // bit = 0: pin is high active (bit order = pin order)
motor.GetConfigurator().Apply(inputs);
```

`Apply()` validates first: the manual forbids the same function on two inputs, and touch
probe on high-speed inputs 1 and 3. A clash writes nothing.

### Reading them

Two views of the same pins:

```cpp
motor.IsNegativeLimitActive();                      // by name
motor.IsInputActive(DigitalInputFunction::kGeneralPurposeA);
motor.GetDigitalInputs().Refresh().GetValue();      // 0x60FD: by FUNCTION, after polarity
motor.GetDigitalInputPins().Refresh().GetValue();   // 0x3141:01: by PIN, before polarity
```

`0x60FD` puts each function at its own bit - the negative limit switch is bit 0 whichever
terminal it is wired to - so code does not change when a switch moves to another pin. The
pin view is the one for checking wiring: it shows what the terminal actually sees.

!!! warning "Polarity and broken wires"
    A normally-closed limit switch with the wrong polarity reads "not triggered" exactly
    when its wire breaks - the failure a limit switch exists to catch. Check each switch by
    hand, with `GetDigitalInputPins()`, before trusting it.

The high-speed inputs do not exist on the Disk 60/8, Disk 60/12 and Micro variants, and are
disabled whenever sensor 2 is configured: they share its pins.

## Digital outputs

Assign a function to a pin once:

```cpp
using epos4::signals::DigitalOutputFunction;

epos4::configs::DigitalOutputConfigs outputs;
outputs.output1 = DigitalOutputFunction::kGeneralPurposeA;
outputs.output2 = DigitalOutputFunction::kHoldingBrake;   // driven by the drive, see below
outputs.polarity = 0x0000;                                // bit = 1: inverted
motor.GetConfigurator().Apply(outputs);
```

Then switch it by function:

```cpp
motor.SetDigitalOutput(DigitalOutputFunction::kGeneralPurposeA, true);
bool on = motor.IsOutputActive(DigitalOutputFunction::kGeneralPurposeA);
motor.GetDigitalOutputs().Refresh();       // 0x60FE:01, by function
motor.GetDigitalOutputPins().Refresh();    // 0x3150:01, by pin, after polarity
```

`SetDigitalOutput()` changes one function and leaves the others. The holding brake and
Ready/Fault outputs belong to the drive and are refused with `std::errc::invalid_argument`.
`kSetBrakeGpio` hands the brake pin over raw - no timing, no standstill interlock - and is an
explicit choice to opt into.

| Output | Function values |
|---|---|
| `output1`, `output2`, `highSpeedOutput1` | `kGeneralPurposeA/B/C`, `kHoldingBrake`, `kReadyFault`, `kSetBrakeGpio`, `kNone` |
| `highSpeedOutput2` | Disk 60/8 and 60/12 only; `kHoldingBrake` or `kNone` |

## Analog inputs

```cpp
auto & voltage = motor.GetAnalogInputVoltage(epos4::signals::AnalogInput::k1).Refresh();
if (!voltage.GetStatus()) {
  double volts = voltage.GetValue().value();
}
```

Each analog input carries a function (`AnalogInputConfigs::input1`, `input2`):

| Function | |
|---|---|
| `kGeneralPurposeA`, `kGeneralPurposeB` | read through `GetAnalogInputGeneralPurpose(AnalogGeneralPurpose::kA)` |
| `kCurrentSetValue` | the input commands current, scaled by the current set-value line |
| `kVelocitySetValue` | the input commands velocity, scaled by the velocity set-value line |
| `kNone` | |

An input used as a set value takes over from the bus: in CSV with a velocity input the
velocity offset is forced to 0, and likewise the torque offset in CST. `AnalogInputConfigs`
also holds the calibration (offset in mV, gain) and the two points of each set-value line;
`Apply()` writes the scaling **before** the functions, so an input never commands through a
stale slope.

## Analog outputs

```cpp
epos4::configs::AnalogOutputConfigs aout;
aout.output1 = epos4::signals::AnalogOutputFunction::kGeneralPurposeA;
motor.GetConfigurator().Apply(aout);

motor.SetAnalogOutput(epos4::signals::AnalogGeneralPurpose::kA, 1.5_V);   // ±4 V
motor.GetAnalogOutputVoltage(epos4::signals::AnalogOutput::k1).Refresh();
```

Beyond ±4 V, `SetAnalogOutput()` returns `std::errc::argument_out_of_domain` rather than
clamping. Output 2 does not exist on the Disk and Micro variants.

## Touch probe

Touch probe 1 latches the **actual position at an edge**, inside the drive, to the
resolution of the encoder - far finer than anything sampled over the bus. For measuring
where a sensor switches, or catching the index pulse.

```cpp
struct TouchProbe
{
  Trigger trigger{Trigger::kInput};   // kInput, kIndexPulse, kSourceObject
  bool continuous{false};             // every edge, not only the first
  bool positiveEdge{true};
  bool negativeEdge{false};
};
```

```cpp
auto probe = epos4::controls::TouchProbe{}
  .WithTrigger(epos4::controls::TouchProbe::Trigger::kInput)   // the input mapped to kTouchProbe
  .WithPositiveEdge(true);
motor.ArmTouchProbe(probe);

// ... move past the sensor ...

epos4::signals::TouchProbeState state;
motor.GetTouchProbe(state);
if (state.positiveEdgeStored) {
  std::int32_t where = state.positiveEdgePosition;   // quadcounts
}
motor.DisarmTouchProbe();   // also clears the edge counters
```

`ArmTouchProbe()` refuses, with `invalid_argument`, a combination the manual rules out (no
edge at all; both edges on the index pulse) and - for `kInput` - a probe with no input mapped
to `kTouchProbe`, which would wait for an edge that can never come. Touch probe runs
alongside any mode except Homing, which clears every latched value when it completes.

| `TouchProbeState` field | |
|---|---|
| `enabled` | the probe is armed |
| `positiveEdgeStored`, `negativeEdgeStored` | a position was latched - check before reading it |
| `positiveEdgePosition`, `negativeEdgePosition` | the latched positions, quadcounts |
| `positiveEdgeCount`, `negativeEdgeCount` | edges since arming |

The complete program: [digital I/O and touch probe](../examples/digital-io.md).
