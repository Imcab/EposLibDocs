// multi_drive: several drives on one bus, commanded together.
//
//   multi_drive <master.dcf> [interface=can0]
//
// Uses config/two_drives/bus.yml: nodes 2 and 3, a left and a right wheel.
// Both receive their setpoint in the same SYNC, so they act on the same
// instant.

#include <array>
#include <chrono>
#include <csignal>
#include <cstdio>
#include <string>
#include <thread>

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
    std::fprintf(stderr, "usage: %s <master.dcf> [interface]\n", argv[0]);
    return 2;
  }
  std::signal(SIGINT, OnSignal);

  // One bus, one master, one socket - shared by every device on it.
  epos4::CanBus bus{{argc > 2 ? argv[2] : "can0", argv[1], 1}};

  // Every device declared before Start().
  epos4::Epos4 left{bus, 2};
  epos4::Epos4 right{bus, 3};
  std::array<epos4::Epos4 *, 2> wheels{&left, &right};

  bus.Start();

  for (auto * wheel : wheels) {
    if (!wheel->WaitUntilReady()) {
      std::printf("node %u does not answer\n", wheel->GetNodeId());
      return 1;
    }
  }

  // Diagnostics from every drive at once: SDO reads to different nodes
  // overlap, so this costs about one round trip instead of two.
  epos4::signals::RefreshAll(left.GetSupplyVoltage(), right.GetSupplyVoltage());
  std::printf("supply: left %.1f V, right %.1f V\n",
    left.GetSupplyVoltage().GetValue().value(), right.GetSupplyVoltage().GetValue().value());

  for (auto * wheel : wheels) {
    if (!wheel->Enable() || wheel->EnterCyclicVelocityMode()) {
      std::printf("node %u: %s\n", wheel->GetNodeId(), wheel->DescribeLastError().c_str());
      for (auto * w : wheels) {w->Disable();}
      return 1;
    }
  }

  // Drive forward, then turn in place, then stop.
  const auto start = std::chrono::steady_clock::now();
  auto next = start;
  for (int cycle = 0; !gStop; ++cycle) {
    const double t = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
    std::int32_t l = 0;
    std::int32_t r = 0;
    if (t < 2.0) {l = 800; r = 800;}            // forward
    else if (t < 4.0) {l = 600; r = -600;}      // turn in place
    else {break;}

    left.StageTargetVelocity(l);
    right.StageTargetVelocity(r);

    if (!left.IsCyclicHealthy() || !right.IsCyclicHealthy()) {
      std::printf("a wheel stopped responding\n");
      break;
    }
    if (cycle % 50 == 0) {
      std::printf("  t=%4.2fs  left %5d rpm  right %5d rpm\n",
        t, left.GetCachedVelocity(), right.GetCachedVelocity());
    }
    next += kPeriod;
    std::this_thread::sleep_until(next);
  }

  for (auto * wheel : wheels) {wheel->StageTargetVelocity(0);}
  std::this_thread::sleep_for(500ms);
  for (auto * wheel : wheels) {
    wheel->ExitCyclicMode();
    wheel->Disable();
  }
  bus.Stop();
  return 0;
}
