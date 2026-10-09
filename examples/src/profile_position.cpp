// profile_position: point-to-point moves, with the drive generating the ramp.
//
//   profile_position <master.dcf> [interface=can0] [node-id=2]
//
// Moves to an absolute position in quadcounts, then a relative move given as
// an angle at the output, then back to where it started.

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <thread>

#include <ros2units/units.h>

#include "epos4/hardware/Epos4.hpp"

using namespace std::chrono_literals;
using namespace units::literals;

namespace
{

// Target reached can still be set from the previous move when a new one is
// commanded: wait for it to drop, then for it to come back.
bool WaitForTarget(epos4::Epos4 & motor, std::chrono::seconds timeout)
{
  for (int i = 0; i < 20 && motor.IsTargetReached().GetValueRefreshed(); ++i) {
    std::this_thread::sleep_for(20ms);
  }
  const auto deadline = std::chrono::steady_clock::now() + timeout;
  while (std::chrono::steady_clock::now() < deadline) {
    if (motor.IsTargetReached().GetValueRefreshed()) {
      return true;
    }
    if (motor.IsFaulted()) {
      std::printf("fault during the move:\n%s\n", motor.DescribeLastError().c_str());
      return false;
    }
    std::this_thread::sleep_for(10ms);
  }
  return false;
}

}  // namespace

int main(int argc, char ** argv)
{
  if (argc < 2) {
    std::fprintf(stderr, "usage: %s <master.dcf> [interface] [node-id]\n", argv[0]);
    return 2;
  }
  epos4::CanBus bus{{argc > 2 ? argv[2] : "can0", argv[1], 1}};
  epos4::Epos4 motor{bus, static_cast<std::uint8_t>(argc > 3 ? std::atoi(argv[3]) : 2)};

  // 500-pulse encoder (2000 quadcounts per motor turn) behind a 1:100 gearbox.
  // Only needed for the requests given as quantities.
  motor.SetMechanism(2000, 1.0 / 100.0);

  bus.Start();
  if (!motor.WaitUntilReady()) {
    std::printf("no answer from the drive\n");
    return 1;
  }

  // The defaults every move uses unless the request overrides them.
  epos4::configs::MotionProfileConfigs profile;
  profile.profileVelocity = 1000;       // rpm
  profile.profileAcceleration = 5000;   // rpm/s
  profile.profileDeceleration = 5000;   // rpm/s
  if (auto ec = motor.GetConfigurator().Apply(profile)) {
    std::printf("profile not applied: %s\n", ec.message().c_str());
    return 1;
  }

  if (!motor.Enable()) {
    std::printf("enable failed: %s\n", motor.DescribeLastError().c_str());
    return 1;
  }
  const std::int32_t start = motor.GetPosition().Refresh().GetValue();

  // 1. Absolute, in raw drive units.
  auto ec = motor.SetControl(epos4::controls::ProfilePosition{}
                               .WithPosition(start + 10000));
  if (ec || !WaitForTarget(motor, 30s)) {
    std::printf("move 1 failed: %s\n", ec.message().c_str());
    motor.Disable();
    return 1;
  }
  std::printf("move 1 done at %d qc\n", motor.GetPosition().Refresh().GetValue());

  // 2. Relative, as an angle at the output, with its own speed.
  ec = motor.SetControl(epos4::controls::ProfilePosition{}
                          .WithPosition(90_deg)     // 90 degrees at the joint
                          .WithVelocity(1500)       // rpm at the motor
                          .WithRelative(true));
  if (ec || !WaitForTarget(motor, 60s)) {
    std::printf("move 2 failed: %s\n", ec.message().c_str());
    motor.Disable();
    return 1;
  }
  std::printf("move 2 done at %d qc\n", motor.GetPosition().Refresh().GetValue());

  // 3. Back to the start.
  ec = motor.SetControl(epos4::controls::ProfilePosition{}.WithPosition(start));
  if (ec || !WaitForTarget(motor, 60s)) {
    std::printf("move 3 failed: %s\n", ec.message().c_str());
  } else {
    std::printf("back at %d qc\n", motor.GetPosition().Refresh().GetValue());
  }

  motor.Disable();
  bus.Stop();
  return 0;
}
