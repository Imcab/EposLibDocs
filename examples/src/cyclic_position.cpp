// cyclic_position: follow a trajectory computed on the master, Cyclic
// Synchronous Position. Here a sine wave around the starting point, in
// degrees at the output.
//
//   cyclic_position <master.dcf> [interface=can0] [node-id=2] [amplitude-deg=20]
//
// The pattern under any trajectory follower: one setpoint per SYNC, the
// drive interpolating between them.

#include <chrono>
#include <cmath>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <thread>

#include <ros2units/units.h>

#include "epos4/hardware/Epos4.hpp"

using namespace std::chrono_literals;

namespace
{
volatile std::sig_atomic_t gStop = 0;
void OnSignal(int) {gStop = 1;}

constexpr auto kPeriod = 10ms;
constexpr double kPi = 3.14159265358979323846;
}  // namespace

int main(int argc, char ** argv)
{
  if (argc < 2) {
    std::fprintf(stderr, "usage: %s <master.dcf> [interface] [node-id] [amplitude-deg]\n",
      argv[0]);
    return 2;
  }
  const double amplitude = argc > 4 ? std::atof(argv[4]) : 20.0;
  std::signal(SIGINT, OnSignal);

  epos4::CanBus bus{{argc > 2 ? argv[2] : "can0", argv[1], 1}};
  epos4::Epos4 motor{bus, static_cast<std::uint8_t>(argc > 3 ? std::atoi(argv[3]) : 2)};
  motor.SetMechanism(2000, 1.0 / 100.0);   // quadcounts per motor turn, gear ratio
  bus.Start();

  if (!motor.WaitUntilReady()) {
    std::printf("no answer from the drive\n");
    return 1;
  }
  for (int i = 0; i < 40 && !motor.IsPdoActive(); ++i) {std::this_thread::sleep_for(50ms);}

  epos4::configs::CyclicConfigs cyclic;
  cyclic.interpolationTimePeriodMs = 10;   // = SYNC period
  motor.GetConfigurator().Apply(cyclic);

  if (!motor.Enable()) {
    std::printf("enable failed: %s\n", motor.DescribeLastError().c_str());
    return 1;
  }
  // Seeds the target with the actual position: the first SYNC holds the axis.
  if (auto ec = motor.EnterCyclicPositionMode()) {
    std::printf("cannot enter CSP: %s\n", ec.message().c_str());
    motor.Disable();
    return 1;
  }

  const auto & mechanism = motor.GetMechanism();
  const units::angle::degree_t origin = mechanism.ToAngle(motor.GetCachedPosition());

  const auto start = std::chrono::steady_clock::now();
  auto next = start;
  for (int cycle = 0; !gStop; ++cycle) {
    const double t = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
    if (t > 4.0) {break;}   // two periods of a 0.5 Hz sine

    const units::angle::degree_t target =
      origin + units::angle::degree_t{amplitude * std::sin(2.0 * kPi * 0.5 * t)};
    motor.StageTargetPosition(target);   // converted to quadcounts, no bus access

    if (!motor.IsCyclicHealthy()) {
      std::printf("axis stopped responding\n");
      break;
    }
    if (cycle % 25 == 0) {
      const units::angle::degree_t actual = mechanism.ToAngle(motor.GetCachedPosition());
      std::printf("  t=%4.2fs  target=%7.2f deg  actual=%7.2f deg\n",
        t, target.value(), actual.value());
    }
    next += kPeriod;
    std::this_thread::sleep_until(next);
  }

  // Back to the origin and hold there for a moment.
  motor.StageTargetPosition(origin);
  std::this_thread::sleep_for(300ms);

  motor.ExitCyclicMode();
  motor.Disable();
  bus.Stop();
  return 0;
}
