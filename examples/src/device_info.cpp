// device_info: everything worth checking on a drive before moving it.
// Read-only: it never enables the motor or changes a parameter.
//
//   device_info <master.dcf> [interface=can0] [node-id=2]

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <thread>
#include <vector>

#include "epos4/hardware/Epos4.hpp"

using namespace std::chrono_literals;

int main(int argc, char ** argv)
{
  if (argc < 2) {
    std::fprintf(stderr, "usage: %s <master.dcf> [interface] [node-id]\n", argv[0]);
    return 2;
  }
  const std::string dcf = argv[1];
  const std::string iface = argc > 2 ? argv[2] : "can0";
  const auto node = static_cast<std::uint8_t>(argc > 3 ? std::atoi(argv[3]) : 2);

  epos4::CanBus bus{{iface, dcf, 1}};
  epos4::Epos4 motor{bus, node};
  bus.Start();

  if (!motor.WaitUntilReady()) {
    std::printf("node %u does not answer: check wiring, bit rate and node-ID\n", node);
    return 1;
  }

  // How the master's boot of the node went. A failed boot still answers SDO.
  for (int i = 0; i < 40 && motor.GetBootStatus().count == 0; ++i) {
    std::this_thread::sleep_for(50ms);
  }
  const auto boot = motor.GetBootStatus();
  std::printf("boot     : %s", boot.Succeeded() ? "ok" : "FAILED");
  if (boot.errorStatus != 0) {
    std::printf(" ('%c' %s)", boot.errorStatus, boot.what.c_str());
  }
  std::printf("\n");

  epos4::signals::DeviceIdentity identity;
  if (!motor.GetIdentity(identity)) {
    std::printf("identity : %s\n", epos4::signals::Describe(identity).c_str());
  }

  // PDOs arrive once the master has configured the node.
  for (int i = 0; i < 40 && !motor.IsPdoActive(); ++i) {
    std::this_thread::sleep_for(50ms);
  }
  std::printf("pdo      : %s\n", motor.IsPdoActive() ? "active" : "NOT ACTIVE (SDO only)");

  std::vector<epos4::signals::PdoMismatch> mismatches;
  if (auto ec = motor.CheckPdoMapping(mismatches)) {
    std::printf("mapping  : not checked (%s)\n", ec.message().c_str());
  } else if (mismatches.empty()) {
    std::printf("mapping  : drive and master agree\n");
  } else {
    std::printf("mapping  : %zu differences\n", mismatches.size());
    for (const auto & m : mismatches) {
      std::printf("  %s\n", epos4::signals::Describe(m).c_str());
    }
  }

  std::printf("state    : %s\n",
    epos4::signals::ToString(motor.GetState().Refresh().GetValue()));
  std::printf("mode     : %s\n",
    epos4::signals::ToString(motor.GetOperationMode().Refresh().GetValue()));

  if (motor.IsFaulted()) {
    std::printf("FAULT:\n%s\n", motor.DescribeLastError().c_str());
  }
  std::vector<std::uint16_t> history;
  if (!motor.GetErrorHistory(history) && !history.empty()) {
    std::printf("history  :");
    for (auto code : history) {std::printf(" 0x%04X", code);}
    std::printf("\n");
  }

  // Several reads at once; with one drive they queue on its SDO channel.
  auto & supply = motor.GetSupplyVoltage();
  auto & temperature = motor.GetPowerStageTemperature();
  auto & current = motor.GetMotorCurrentAveraged();
  epos4::signals::RefreshAll(supply, temperature, current);
  if (epos4::signals::IsAllGood(supply, temperature, current)) {
    std::printf("supply   : %.1f V\n", supply.GetValue().value());
    std::printf("stage    : %.1f degC\n", temperature.GetValue().value());
    std::printf("current  : %.3f A\n", current.GetValue().value());
  }

  epos4::configs::MotorConfigs motorData;
  if (!motor.GetConfigurator().Refresh(motorData)) {
    if (motorData.nominalCurrent) {
      std::printf("nominal  : %.3f A\n", *motorData.nominalCurrent / 1000.0);
    }
    if (motorData.torqueConstant) {
      std::printf("Kt       : %.2f mNm/A\n", *motorData.torqueConstant / 1000.0);
    }
    if (motorData.ratedTorque) {
      std::printf("rated    : %.4f Nm\n", *motorData.ratedTorque / 1e6);
    }
  }

  std::printf("position : %d qc\n", motor.GetPosition().Refresh().GetValue());
  std::printf("velocity : %d rpm\n", motor.GetVelocity().Refresh().GetValue());

  bus.Stop();
  return 0;
}
