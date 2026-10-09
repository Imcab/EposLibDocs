// fault_monitor: react to faults as they happen, and recover from them.
//
//   fault_monitor <master.dcf> [interface=can0] [node-id=2] [seconds=10]
//
// Registers an emergency callback, polls telemetry at 2 Hz, and clears a
// fault when one arrives - the loop a base station status panel runs.

#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <thread>

#include "epos4/hardware/Epos4.hpp"
#include "epos4/signals/Errors.hpp"

using namespace std::chrono_literals;

namespace
{
volatile std::sig_atomic_t gStop = 0;
void OnSignal(int) {gStop = 1;}
}  // namespace

int main(int argc, char ** argv)
{
  if (argc < 2) {
    std::fprintf(stderr, "usage: %s <master.dcf> [interface] [node-id] [seconds]\n", argv[0]);
    return 2;
  }
  const double seconds = argc > 4 ? std::atof(argv[4]) : 10.0;
  std::signal(SIGINT, OnSignal);

  epos4::CanBus bus{{argc > 2 ? argv[2] : "can0", argv[1], 1}};
  epos4::Epos4 motor{bus, static_cast<std::uint8_t>(argc > 3 ? std::atoi(argv[3]) : 2)};

  // Runs on the CANopen thread the moment an EMCY frame arrives: keep it
  // short, never block, never call back into the device. Here it only flags
  // the main loop. May be registered before Start().
  std::atomic<bool> emergency{false};
  motor.SetEmergencyCallback(
    [&emergency](const epos4::signals::EmergencyMessage & message) {
      std::printf("EMCY: %s\n", epos4::signals::Describe(message).c_str());
      if (message.errorCode != 0 && !epos4::signals::IsWarning(message.errorCode)) {
        emergency = true;
      }
    });

  bus.Start();
  if (!motor.WaitUntilReady()) {
    std::printf("no answer from the drive\n");
    return 1;
  }

  auto & voltage = motor.GetSupplyVoltage();
  auto & temperature = motor.GetPowerStageTemperature();
  auto & i2t = motor.GetMotorI2tPercent();

  const auto end = std::chrono::steady_clock::now() +
    std::chrono::duration_cast<std::chrono::steady_clock::duration>(
    std::chrono::duration<double>(seconds));
  while (!gStop && std::chrono::steady_clock::now() < end) {
    epos4::signals::RefreshAll(voltage, temperature, i2t);
    std::printf("%.1f V  %.1f degC  I2t %u %%  last EMCY 0x%04X\n",
      voltage.GetValue().value(), temperature.GetValue().value(),
      unsigned{i2t.GetValue()}, motor.GetCachedErrorCode());

    if (emergency.exchange(false) || motor.IsFaulted()) {
      // Cause, effect and recovery from chapter 7 of the firmware manual.
      std::printf("%s\n", motor.DescribeLastError().c_str());

      const std::uint16_t code = motor.GetErrorCode().Refresh().GetValue();
      if (epos4::signals::ClearsPosition(code)) {
        std::printf("this fault clears the position: home again before moving\n");
      }
      // Sends the NMT reset communication first when the fault needs it
      // (a lost heartbeat, CAN passive). False if the cause is still there.
      std::printf("clearing... %s\n", motor.ClearFault() ? "ok" : "still faulted");
    }
    std::this_thread::sleep_for(500ms);
  }

  bus.Stop();
  return 0;
}
