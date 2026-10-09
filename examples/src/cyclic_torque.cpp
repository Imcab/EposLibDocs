// cyclic_torque: a torque step through Cyclic Synchronous Torque, with a
// velocity limit as protection.
//
//   cyclic_torque <master.dcf> [interface=can0] [node-id=2] [Nm=0.05] [max-rpm=1000]
//
// The torque is at the MOTOR shaft. On a free shaft any torque above
// friction accelerates the motor until it reaches its no-load speed: block
// the output, or keep the time short.

#include <chrono>
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
}  // namespace

int main(int argc, char ** argv)
{
  if (argc < 2) {
    std::fprintf(stderr, "usage: %s <master.dcf> [interface] [node-id] [Nm] [max-rpm]\n",
      argv[0]);
    return 2;
  }
  const units::torque::newton_meter_t torque{argc > 4 ? std::atof(argv[4]) : 0.05};
  const int maxRpm = argc > 5 ? std::atoi(argv[5]) : 1000;
  std::signal(SIGINT, OnSignal);

  epos4::CanBus bus{{argc > 2 ? argv[2] : "can0", argv[1], 1}};
  epos4::Epos4 motor{bus, static_cast<std::uint8_t>(argc > 3 ? std::atoi(argv[3]) : 2)};
  bus.Start();

  if (!motor.WaitUntilReady()) {
    std::printf("no answer from the drive\n");
    return 1;
  }
  for (int i = 0; i < 40 && !motor.IsPdoActive(); ++i) {std::this_thread::sleep_for(50ms);}

  // Every torque in CST is in thousandths of this. Zero means the motor data
  // (nominal current, torque constant) was never configured.
  auto & rated = motor.GetMotorRatedTorque().Refresh();
  if (rated.GetStatus() || rated.GetValue() == 0) {
    std::printf("motor rated torque unavailable: configure MotorConfigs first\n");
    return 1;
  }
  std::printf("rated torque %.4f Nm, commanding %.4f Nm\n",
    rated.GetValue() / 1e6, torque.value());

  if (!motor.Enable()) {
    std::printf("enable failed: %s\n", motor.DescribeLastError().c_str());
    return 1;
  }
  // Starts at the torque the axis produces now, so a loaded joint is not dropped.
  if (auto ec = motor.EnterCyclicTorqueMode()) {
    std::printf("cannot enter CST: %s\n", ec.message().c_str());
    motor.Disable();
    return 1;
  }

  const auto start = std::chrono::steady_clock::now();
  auto next = start;
  int code = 0;
  for (int cycle = 0; ; ++cycle) {
    const double t = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
    if (gStop || t > 2.0) {break;}

    if (std::abs(motor.GetCachedVelocity()) > maxRpm) {
      std::printf("%d rpm > %d: torque to zero\n", motor.GetCachedVelocity(), maxRpm);
      code = 1;
      break;
    }
    if (!motor.IsCyclicHealthy()) {
      std::printf("axis stopped responding\n");
      code = 1;
      break;
    }

    // In N m: converted with the rated torque read when CST was entered, so
    // there is no bus access here. Raw thousandths work too:
    // StageTargetTorque(std::int16_t{100}).
    motor.StageTargetTorque(torque);

    if (cycle % 20 == 0) {
      std::printf("  t=%4.2fs  torque=%5d per mille  velocity=%6d rpm\n",
        t, motor.GetCachedTorque(), motor.GetCachedVelocity());
    }
    next += kPeriod;
    std::this_thread::sleep_until(next);
  }

  // Zero torque for a few cycles so it reaches the drive before disabling.
  motor.StageTargetTorque(std::int16_t{0});
  std::this_thread::sleep_for(5 * kPeriod);
  motor.ExitCyclicMode();
  motor.Disable();
  bus.Stop();
  return code;
}
