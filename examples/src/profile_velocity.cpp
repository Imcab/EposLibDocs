// profile_velocity: spin at a velocity, with the drive generating the ramp.
//
//   profile_velocity <master.dcf> [interface=can0] [node-id=2] [rpm=1000] [seconds=3]
//
// epos4_sim does not model Profile Velocity Mode: run this on hardware.

#include <chrono>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <thread>

#include "epos4/hardware/Epos4.hpp"

using namespace std::chrono_literals;

namespace
{
volatile std::sig_atomic_t gStop = 0;
void OnSignal(int) {gStop = 1;}
}  // namespace

int main(int argc, char ** argv)
{
  if (argc < 2) {
    std::fprintf(stderr, "usage: %s <master.dcf> [interface] [node-id] [rpm] [seconds]\n",
      argv[0]);
    return 2;
  }
  const int rpm = argc > 4 ? std::atoi(argv[4]) : 1000;
  const double seconds = argc > 5 ? std::atof(argv[5]) : 3.0;
  std::signal(SIGINT, OnSignal);

  epos4::CanBus bus{{argc > 2 ? argv[2] : "can0", argv[1], 1}};
  epos4::Epos4 motor{bus, static_cast<std::uint8_t>(argc > 3 ? std::atoi(argv[3]) : 2)};
  bus.Start();

  if (!motor.WaitUntilReady() || !motor.Enable()) {
    std::printf("not ready: %s\n", motor.DescribeLastError().c_str());
    return 1;
  }

  // Reaches rpm in rpm / acceleration seconds: 1000 rpm at 2000 rpm/s is 0.5 s.
  auto ec = motor.SetControl(epos4::controls::ProfileVelocity{}
                               .WithVelocity(rpm)
                               .WithAcceleration(2000)
                               .WithDeceleration(2000));
  if (ec) {
    std::printf("rejected: %s\n", ec.message().c_str());
    motor.Disable();
    return 1;
  }

  const auto end = std::chrono::steady_clock::now() +
    std::chrono::duration_cast<std::chrono::steady_clock::duration>(
    std::chrono::duration<double>(seconds));
  while (!gStop && std::chrono::steady_clock::now() < end) {
    std::printf("  velocity %5d rpm  current %.3f A\n",
      motor.GetVelocity().Refresh().GetValue(),
      motor.GetMotorCurrentAveraged().Refresh().GetValue().value());
    std::this_thread::sleep_for(200ms);
  }

  // Back to zero on the same deceleration ramp, then wait for standstill.
  motor.SetControl(epos4::controls::ProfileVelocity{}.WithVelocity(0));
  for (int i = 0; i < 100 && !motor.IsAtZeroSpeed().GetValueRefreshed(); ++i) {
    std::this_thread::sleep_for(20ms);
  }

  motor.Disable();
  bus.Stop();
  return 0;
}
