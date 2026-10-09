// digital_io: read the inputs, drive a general purpose output, and latch a
// position with the touch probe.
//
//   digital_io <master.dcf> [interface=can0] [node-id=2]

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <thread>

#include <ros2units/units.h>

#include "epos4/hardware/Epos4.hpp"

using namespace std::chrono_literals;
using namespace units::literals;
using epos4::signals::DigitalInputFunction;
using epos4::signals::DigitalOutputFunction;

int main(int argc, char ** argv)
{
  if (argc < 2) {
    std::fprintf(stderr, "usage: %s <master.dcf> [interface] [node-id]\n", argv[0]);
    return 2;
  }
  epos4::CanBus bus{{argc > 2 ? argv[2] : "can0", argv[1], 1}};
  epos4::Epos4 motor{bus, static_cast<std::uint8_t>(argc > 3 ? std::atoi(argv[3]) : 2)};
  bus.Start();
  if (!motor.WaitUntilReady()) {
    std::printf("no answer from the drive\n");
    return 1;
  }

  // Inputs, two views: by function (after polarity) and by pin (raw).
  std::printf("negative limit %d, positive limit %d, home %d\n",
    motor.IsNegativeLimitActive(), motor.IsPositiveLimitActive(), motor.IsHomeSwitchActive());
  std::printf("functions 0x%08X, pins 0x%04X\n",
    motor.GetDigitalInputs().Refresh().GetValue(),
    motor.GetDigitalInputPins().Refresh().GetValue());
  std::printf("general purpose A: %d\n",
    motor.IsInputActive(DigitalInputFunction::kGeneralPurposeA));

  // Outputs: assign the function to a pin once, then switch it by function.
  epos4::configs::DigitalOutputConfigs outputs;
  outputs.output1 = DigitalOutputFunction::kGeneralPurposeA;
  if (auto ec = motor.GetConfigurator().Apply(outputs)) {
    std::printf("outputs not configured: %s\n", ec.message().c_str());
  }
  motor.SetDigitalOutput(DigitalOutputFunction::kGeneralPurposeA, true);
  std::printf("output A: %d, pins 0x%04X\n",
    motor.IsOutputActive(DigitalOutputFunction::kGeneralPurposeA),
    motor.GetDigitalOutputPins().Refresh().GetValue());
  motor.SetDigitalOutput(DigitalOutputFunction::kGeneralPurposeA, false);

  // Analog channels, in volts.
  auto & ain1 = motor.GetAnalogInputVoltage(epos4::signals::AnalogInput::k1).Refresh();
  if (!ain1.GetStatus()) {
    std::printf("analog input 1: %.3f V\n", ain1.GetValue().value());
  }
  motor.SetAnalogOutput(epos4::signals::AnalogGeneralPurpose::kA, 1.5_V);

  // Touch probe on the encoder index: latches the exact position of the next
  // index pulse, in the drive. Then turn the motor past it.
  const auto probe = epos4::controls::TouchProbe{}
    .WithTrigger(epos4::controls::TouchProbe::Trigger::kIndexPulse)
    .WithPositiveEdge(true);
  if (auto ec = motor.ArmTouchProbe(probe)) {
    std::printf("touch probe not armed: %s\n", ec.message().c_str());
  } else if (motor.Enable()) {
    motor.SetControl(epos4::controls::ProfilePosition{}.WithPosition(4000).WithRelative(true));
    std::this_thread::sleep_for(2s);

    epos4::signals::TouchProbeState state;
    if (!motor.GetTouchProbe(state) && state.positiveEdgeStored) {
      std::printf("index latched at %d qc (%u edges)\n",
        state.positiveEdgePosition, unsigned{state.positiveEdgeCount});
    } else {
      std::printf("no edge latched\n");
    }
    motor.DisarmTouchProbe();
    motor.Disable();
  }

  bus.Stop();
  return 0;
}
