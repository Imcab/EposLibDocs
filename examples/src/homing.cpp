// homing: give an incremental encoder an absolute zero.
//
//   homing <master.dcf> [interface=can0] [node-id=2]
//
// Maps the limit switches to their inputs, configures the homing run and
// homes on the negative limit switch. Then shows SetPosition(), which
// declares a position without moving.

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include "epos4/hardware/Epos4.hpp"

using namespace std::chrono_literals;
using epos4::signals::DigitalInputFunction;
using epos4::signals::HomingMethod;

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

  // Which methods this firmware implements.
  std::vector<HomingMethod> supported;
  if (!motor.GetSupportedHomingMethods(supported)) {
    std::printf("supported:");
    for (auto m : supported) {std::printf(" %d", static_cast<int>(m));}
    std::printf("\n");
  }

  // A switch that is wired but not mapped does nothing. These are also the
  // drive's defaults (Table 6-130); Apply() refuses a function mapped twice.
  epos4::configs::DigitalInputConfigs inputs;
  inputs.input1 = DigitalInputFunction::kNegativeLimitSwitch;
  inputs.input2 = DigitalInputFunction::kPositiveLimitSwitch;
  inputs.input3 = DigitalInputFunction::kHomeSwitch;

  epos4::configs::HomingConfigs homing;
  homing.speedForSwitchSearch = 500;    // rpm
  homing.speedForZeroSearch = 100;      // rpm
  homing.acceleration = 2000;           // rpm/s
  homing.homeOffsetMoveDistance = 0;    // qc to move away from the switch afterwards
  homing.homePosition = 0;              // the position assigned to home

  if (auto ec = motor.GetConfigurator().Apply(inputs)) {
    std::printf("inputs not applied: %s\n", ec.message().c_str());
    return 1;
  }
  if (auto ec = motor.GetConfigurator().Apply(homing)) {
    std::printf("homing not applied: %s\n", ec.message().c_str());
    return 1;
  }

  // A limit switch already active before the run usually means an inverted
  // polarity, not an axis sitting at the end stop.
  std::printf("negative limit active: %s\n", motor.IsNegativeLimitActive() ? "yes" : "no");

  if (!motor.Enable()) {
    std::printf("enable failed: %s\n", motor.DescribeLastError().c_str());
    return 1;
  }

  // Blocks until the run ends. Refuses a method whose switch is not mapped,
  // or that the drive does not list, before anything moves.
  const bool homed = motor.Home(
    epos4::controls::Homing{}.WithMethod(HomingMethod::kNegativeLimitSwitch), 30s);
  std::printf("homing %s, position %d qc, referenced: %s\n",
    homed ? "attained" : "FAILED",
    motor.GetPosition().Refresh().GetValue(),
    motor.IsPositionReferenced().GetValueRefreshed() ? "yes" : "no");
  if (!homed && motor.HasHomingError().GetValueRefreshed()) {
    std::printf("the drive reports a homing error\n");
  }

  // Declare "here is 1000" without moving (homing method 37). The homing
  // configuration is restored afterwards.
  if (auto ec = motor.SetPosition(1000)) {
    std::printf("SetPosition failed: %s\n", ec.message().c_str());
  } else {
    std::printf("position is now %d qc\n", motor.GetPosition().Refresh().GetValue());
  }

  motor.Disable();
  bus.Stop();
  return homed ? 0 : 1;
}
