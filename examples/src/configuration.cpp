// configuration: read a drive's whole configuration, change part of it, and
// make the change permanent.
//
//   configuration <master.dcf> [interface=can0] [node-id=2] [--save]
//
// Without --save the change lasts until the next power cycle.

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include "epos4/hardware/Epos4.hpp"

namespace
{

template<typename T>
void Print(const char * name, const std::optional<T> & value, const char * unit = "")
{
  if (value) {
    std::printf("  %-28s %lld %s\n", name, static_cast<long long>(*value), unit);
  } else {
    std::printf("  %-28s (not read)\n", name);
  }
}

}  // namespace

int main(int argc, char ** argv)
{
  if (argc < 2) {
    std::fprintf(stderr, "usage: %s <master.dcf> [interface] [node-id] [--save]\n", argv[0]);
    return 2;
  }
  const bool save = argc > 4 && std::strcmp(argv[4], "--save") == 0;

  epos4::CanBus bus{{argc > 2 ? argv[2] : "can0", argv[1], 1}};
  epos4::Epos4 motor{bus, static_cast<std::uint8_t>(argc > 3 ? std::atoi(argv[3]) : 2)};
  bus.Start();
  if (!motor.WaitUntilReady()) {
    std::printf("no answer from the drive\n");
    return 1;
  }
  auto & configurator = motor.GetConfigurator();

  // 1. Read everything. Keeps going past objects that cannot be read (absent
  //    on this hardware or firmware) and returns the first error.
  epos4::configs::Epos4Configuration current;
  if (auto ec = configurator.Refresh(current)) {
    std::printf("some objects could not be read: %s\n", ec.message().c_str());
  }
  std::printf("before:\n");
  Print("limits.maxMotorSpeed", current.limits.maxMotorSpeed, "rpm");
  Print("motionProfile.profileAcceleration", current.motionProfile.profileAcceleration, "rpm/s");
  Print("velocityControl.p", current.velocityControl.p);
  Print("velocityControl.i", current.velocityControl.i);
  Print("motor.nominalCurrent", current.motor.nominalCurrent, "mA");

  // 2. Change only what matters. Unset fields are not written, so the gains
  //    and the motor data above stay exactly as they are.
  epos4::configs::Epos4Configuration change;
  change.limits.maxMotorSpeed = 4000;
  change.motionProfile.profileAcceleration = 3000;
  change.motionProfile.profileDeceleration = 3000;
  change.stopOptions.faultReaction = epos4::signals::FaultReactionOption::kSlowDownOnQuickStopRamp;

  // Validates first and writes nothing if the input mappings clash. Groups
  // that need «Power Disable» are refused while the motor is powered.
  if (auto ec = configurator.Apply(change)) {
    std::printf("apply failed: %s\n", ec.message().c_str());
    return 1;
  }

  // 3. Read back what landed.
  epos4::configs::LimitConfigs limits;
  epos4::configs::MotionProfileConfigs profile;
  configurator.Refresh(limits);
  configurator.Refresh(profile);
  std::printf("after:\n");
  Print("limits.maxMotorSpeed", limits.maxMotorSpeed, "rpm");
  Print("motionProfile.profileAcceleration", profile.profileAcceleration, "rpm/s");

  // 4. Persist across power cycles (0x1010).
  if (save) {
    auto ec = configurator.Save();
    std::printf("save: %s\n", ec ? ec.message().c_str() : "ok");
  }

  bus.Stop();
  return 0;
}
