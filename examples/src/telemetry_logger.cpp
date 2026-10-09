// telemetry_logger: record a drive's health to CSV - supply, temperature,
// current, I2t, PWM duty cycle, state and the last EMCY - at a fixed rate.
//
//   telemetry_logger <master.dcf> <interface> <node-id> <out.csv> [seconds=10] [hz=5]
//
// What a rover logs on every drive during a run, to answer afterwards why a
// joint stopped. Read-only.

#include <chrono>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <thread>

#include "epos4/hardware/Epos4.hpp"

using namespace std::chrono_literals;
using Clock = std::chrono::steady_clock;

namespace
{
volatile std::sig_atomic_t gStop = 0;
void OnSignal(int) {gStop = 1;}
}  // namespace

int main(int argc, char ** argv)
{
  if (argc < 5) {
    std::fprintf(stderr, "usage: %s <master.dcf> <interface> <node-id> <out.csv> [seconds] [hz]\n",
      argv[0]);
    return 2;
  }
  const double seconds = argc > 5 ? std::atof(argv[5]) : 10.0;
  const double hz = argc > 6 ? std::atof(argv[6]) : 5.0;
  std::signal(SIGINT, OnSignal);

  epos4::CanBus bus{{argv[2], argv[1], 1}};
  epos4::Epos4 motor{bus, static_cast<std::uint8_t>(std::atoi(argv[3]))};
  bus.Start();
  if (!motor.WaitUntilReady()) {
    std::fprintf(stderr, "no answer from the drive\n");
    return 1;
  }

  std::FILE * file = std::fopen(argv[4], "w");
  if (!file) {
    std::fprintf(stderr, "cannot write %s\n", argv[4]);
    return 1;
  }
  std::fprintf(file, "t,supply_V,stage_degC,current_A,i2t_motor_pct,i2t_stage_pct,"
    "pwm_permille,state,last_emcy\n");

  auto & supply = motor.GetSupplyVoltage();
  auto & stage = motor.GetPowerStageTemperature();
  auto & current = motor.GetMotorCurrentAveraged();
  auto & i2tMotor = motor.GetMotorI2tPercent();
  auto & i2tStage = motor.GetPowerStageI2tPercent();
  auto & pwm = motor.GetPwmDutyCyclePerMille();
  auto & state = motor.GetState();

  const auto period = std::chrono::duration_cast<Clock::duration>(
    std::chrono::duration<double>(1.0 / hz));
  const auto start = Clock::now();
  auto next = start;
  int rows = 0;
  while (!gStop) {
    const double t = std::chrono::duration<double>(Clock::now() - start).count();
    if (t > seconds) {break;}

    // One concurrent refresh for everything; values that failed keep their
    // last good reading and are marked by an empty field.
    epos4::signals::RefreshAll(supply, stage, current, i2tMotor, i2tStage, pwm, state);
    auto field = [](const auto & signal, auto value) {
        return signal.GetStatus() ? std::string{} : std::to_string(value);
      };
    std::fprintf(file, "%.2f,%s,%s,%s,%s,%s,%s,%s,0x%04X\n", t,
      field(supply, supply.GetValue().value()).c_str(),
      field(stage, stage.GetValue().value()).c_str(),
      field(current, current.GetValue().value()).c_str(),
      field(i2tMotor, i2tMotor.GetValue()).c_str(),
      field(i2tStage, i2tStage.GetValue()).c_str(),
      field(pwm, pwm.GetValue()).c_str(),
      state.GetStatus() ? "" : epos4::signals::ToString(state.GetValue()),
      motor.GetCachedErrorCode());
    std::fflush(file);
    ++rows;

    next += period;
    std::this_thread::sleep_until(next);
  }

  std::fclose(file);
  bus.Stop();
  std::printf("%d rows written to %s\n", rows, argv[4]);
  return 0;
}
