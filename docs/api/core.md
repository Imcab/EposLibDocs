# The core library

`eposlib::core` is the logic of the library with no CAN and no Lely: the CiA 402 state
machine, the object dictionary, units, configuration groups, error tables, the PDO mapping
decoder and the cyclic state. It is what the tests run against, and it is usable on its own
- in a unit test, a log analyser, or a tool that decodes captured frames.

```cmake
target_link_libraries(my_tool PRIVATE eposlib::core)
```

## State machine

```cpp
#include "epos4/core/Cia402StateMachine.hpp"

std::optional<epos4::signals::State> s = epos4::core::Decode(0x0637);
// kOperationEnabled - bits 7 and 4 are masked off, as Table 2-5 marks them "x"

epos4::core::Controlword cw;
cw.Apply(epos4::core::Command::kShutdown);           // read-modify-write of Table 2-7
cw.Apply(epos4::core::Command::kSwitchOnAndEnableOperation);
cw.SetModeBits(epos4::signals::control_bits::kNewSetpoint);   // mode bits only
std::uint16_t raw = cw.Raw();

auto step = epos4::core::PlanStep(*s, epos4::core::Goal::kDisabled);
// step.progress: kReached / kInProgress / kBlocked; step.command: what to send now
```

- `Decode()` returns `std::nullopt` for a pattern that matches none of the eight states.
- `Controlword::Apply()` touches only the bits Table 2-7 pins for each command, keeps the
  operating-mode bits, and lowers bit 7 first so that two calls produce the fault-reset edge.
  `SetModeBits()` / `ClearModeBits()` cannot touch a state machine bit by construction.
- `PlanStep(state, goal)` answers "what do I send this cycle": stateless, never blocks,
  never emits a fault reset.

## Errors, identity, PDO mapping

The tables of [Diagnostics](diagnostics.md) are all in core: `DescribeDeviceError()`,
`FindAbortCode()`, `HardwareName()`, `FirmwareName()`. So is the PDO decoder:

```cpp
#include "epos4/signals/PdoMapping.hpp"

auto writes = epos4::signals::ParseConciseDcf(bytesOfNodeBin);   // a node_N.bin
epos4::signals::PdoObject o = epos4::signals::PdoObject::Decode(0x60410010);
// o.index == 0x6041, o.subindex == 0, o.bits == 16
```

`CompareWithConciseDcf(concise, mapping)` is the comparison `CheckPdoMapping()` performs.

## Units and setpoints

`MechanismScale`, `ToTorque()`, `ToPerThousand()`, `ToCurrent()`, `ToVoltage()`,
`ToTemperature()` (see [Units](../concepts/units.md)), and the setpoint types with their
`Resolve()` functions:

```cpp
epos4::MechanismScale scale{2000, 1.0 / 100.0};
std::int32_t counts{};
bool ok = epos4::Resolve(epos4::PositionSetpoint{units::angle::degree_t{90.0}}, scale, counts);
// counts == 50000
```

## Configuration groups

Each group can produce its writes and parse its reads without a device:

```cpp
epos4::configs::LimitConfigs limits;
limits.maxMotorSpeed = 4000;
epos4::configs::ConfigWrites writes;
limits.AppendTo(writes);        // [{0x6080:00, 4000u}]
```

`Epos4Configuration::ToWrites()` and `Validate()` work the same way - useful to check a
configuration in a test before it ever reaches a drive.

## The cyclic state

`core::CyclicState` is the state the cyclic path shares between the control thread and the
bus thread - staged setpoints, cached feedback, the health rule - with every member
statically asserted lock-free. The clock is passed in, so "unhealthy once PDOs stop" is
testable between two time points instead of by sleeping.
