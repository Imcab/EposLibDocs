// encoder_setup: inspect the feedback configuration, read the resolution the
// drive computed, and report the position in degrees at the output.
//
//   encoder_setup <master.dcf> [interface=can0] [node-id=2] [gear-ratio=0.01]
//
// Read-only. Writing encoder configuration (Encoder::Apply) needs the motor
// unpowered and clears the homing reference.

#include <cstdio>
#include <cstdlib>
#include <string>

#include <ros2units/units.h>

#include "epos4/hardware/Encoder.hpp"
#include "epos4/hardware/Epos4.hpp"

int main(int argc, char ** argv)
{
  if (argc < 2) {
    std::fprintf(stderr, "usage: %s <master.dcf> [interface] [node-id] [gear-ratio]\n",
      argv[0]);
    return 2;
  }
  const double gearRatio = argc > 4 ? std::atof(argv[4]) : 1.0 / 100.0;

  epos4::CanBus bus{{argc > 2 ? argv[2] : "can0", argv[1], 1}};
  epos4::Epos4 motor{bus, static_cast<std::uint8_t>(argc > 3 ? std::atoi(argv[3]) : 2)};
  bus.Start();
  if (!motor.WaitUntilReady()) {
    std::printf("no answer from the drive\n");
    return 1;
  }
  auto & encoder = motor.GetEncoder();

  // Which sensor sits in each slot (0x3000:01), and the main resolution.
  epos4::configs::SensorsConfigs sensors;
  if (auto ec = encoder.Refresh(sensors)) {
    std::printf("sensors not read: %s\n", ec.message().c_str());
  } else {
    std::printf("sensor 1: 0x%02X  sensor 2: 0x%02X  sensor 3: 0x%02X\n",
      static_cast<unsigned>(sensors.sensor1.value_or(epos4::configs::Sensor1Type::kNone)),
      static_cast<unsigned>(sensors.sensor2.value_or(epos4::configs::Sensor2Type::kNone)),
      static_cast<unsigned>(sensors.sensor3.value_or(epos4::configs::Sensor3Type::kNone)));
    if (sensors.mainSensorResolution) {
      std::printf("main sensor resolution: %u qc/rev\n", *sensors.mainSensorResolution);
    }
  }

  // The incremental encoder 1 settings.
  epos4::configs::DigitalIncrementalEncoderConfigs incremental;
  incremental.encoderNumber = 1;
  if (!encoder.Refresh(incremental) && incremental.pulsesPerRevolution) {
    std::printf("encoder 1: %u pulses/rev = %u qc/rev\n",
      *incremental.pulsesPerRevolution,
      epos4::Encoder::QuadCountsPerRevolution(*incremental.pulsesPerRevolution));
  }

  // Take the resolution from the drive; only the gear ratio is ours. If the
  // drive does not report one, compute it from the encoder's pulses.
  if (auto ec = encoder.SetMechanismFromDevice(gearRatio)) {
    if (!incremental.pulsesPerRevolution) {
      std::printf("mechanism not set: %s\n", ec.message().c_str());
      return 1;
    }
    encoder.SetMechanism(
      epos4::Encoder::QuadCountsPerRevolution(*incremental.pulsesPerRevolution), gearRatio);
    std::printf("resolution from the encoder pulses: %u qc/rev\n",
      encoder.GetMechanism().GetQuadCountsPerRevolution());
  }
  if (auto angle = encoder.GetAngle()) {
    const units::angle::degree_t degrees = *angle;
    std::printf("output angle: %.3f deg\n", degrees.value());
  }
  if (auto velocity = encoder.GetAngularVelocity()) {
    std::printf("output velocity: %.3f rpm\n", velocity->value());
  }

  bus.Stop();
  return 0;
}
