// Compile-only check of the API calls the documentation pages show. Never run:
// it only has to build, so that a page cannot show a call that does not exist
// or a type that does not convert.

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

#include <ros2units/units.h>

#include "epos4/Version.hpp"
#include "epos4/core/Cia402StateMachine.hpp"
#include "epos4/hardware/Encoder.hpp"
#include "epos4/hardware/Epos4.hpp"
#include "epos4/signals/PdoMapping.hpp"

using namespace std::chrono_literals;
using namespace units::literals;
using epos4::signals::DigitalInputFunction;
using epos4::signals::DigitalOutputFunction;

void Lifecycle()
{
  epos4::CanBus::Options options;
  options.interface = "can0";
  options.masterDcf = "master.dcf";
  epos4::CanBus bus{options};
  epos4::CanBus bus2{{"can0", "master.dcf", 1}};
  epos4::Epos4 motor{bus, 2};
  motor.SetMechanism(4096, 1.0 / 160.0);
  motor.SetEmergencyCallback([](const epos4::signals::EmergencyMessage & m) {
      std::printf("%s\n", epos4::signals::Describe(m).c_str());
    });
  bus.Start();
  motor.WaitUntilReady(std::chrono::seconds{5});
  const epos4::signals::BootStatus boot = motor.GetBootStatus();
  std::printf("%u %c %s %d\n", boot.count, boot.errorStatus, boot.what.c_str(), boot.Succeeded());
  bus.Stop();
  bus.Start();
}

void Enabling(epos4::Epos4 & motor)
{
  if (!motor.Enable()) {
    if (motor.IsFaulted()) {std::printf("%s\n", motor.DescribeLastError().c_str());}
  }
  motor.Enable(1000ms);
  motor.Disable(1000ms);
  motor.ClearFault(3000ms);
  motor.Halt();
  motor.SetControl(epos4::controls::Halt{});
  motor.QuickStop();
  motor.IsEnabled();
  const auto code = motor.GetErrorCode().Refresh().GetValue();
  epos4::signals::ClearsPosition(code);
  std::printf("%s\n", epos4::signals::ToString(motor.GetState().Refresh().GetValue()));
  epos4::signals::IsPowerDisabled(motor.GetState().GetValue());
}

void Requests(epos4::Epos4 & motor)
{
  motor.SetControl(epos4::controls::ProfilePosition{}.WithPosition(50000));
  motor.SetControl(epos4::controls::ProfilePosition{}
                     .WithPosition(90_deg)
                     .WithVelocity(1500)
                     .WithAcceleration(8000)
                     .WithDeceleration(8000)
                     .WithRelative(true)
                     .WithChangeSetImmediately(false));
  motor.SetControl(epos4::controls::ProfilePosition{}.WithPosition(units::angle::degree_t{90.0}));
  motor.SetControl(epos4::controls::ProfileVelocity{}
                     .WithVelocity(1000).WithAcceleration(2000).WithDeceleration(2000));
  motor.SetControl(epos4::controls::ProfileVelocity{}.WithVelocity(0));
  motor.IsAtZeroSpeed().GetValueRefreshed();
  motor.IsTargetReached().GetValueRefreshed();
  motor.HasFollowingError().Refresh();
  motor.SupportsMode(epos4::signals::OperationMode::kCyclicSynchronousTorque);
}

void Cyclic(epos4::Epos4 & motor)
{
  motor.EnterCyclicPositionMode();
  motor.EnterCyclicVelocityMode();
  motor.EnterCyclicTorqueMode();
  motor.StageTargetPosition(50000);
  motor.StageTargetPosition(90_deg);
  motor.StageTargetVelocity(1500);
  motor.StageTargetVelocity(15_rpm);
  motor.StageTargetTorque(std::int16_t{250});
  motor.StageTargetTorque(250);
  motor.StageTargetTorque(0.5_Nm);
  motor.GetCachedPosition();
  motor.GetCachedVelocity();
  motor.GetCachedTorque();
  motor.GetCachedStatusword();
  motor.GetCachedErrorCode();
  motor.GetTimeSinceLastEmergency();
  motor.IsCyclicHealthy();
  motor.IsCyclicHealthy(50ms);
  motor.IsCyclicModeActive();
  motor.ExitCyclicMode();
  auto & rated = motor.GetMotorRatedTorque().Refresh();
  (void)rated;
  std::optional<std::int16_t> pm = motor.TorqueToPerThousand(0.1_Nm);
  std::optional<units::torque::newton_meter_t> t = motor.PerThousandToTorque(150);
  (void)pm;
  (void)t;
  motor.SetControl(epos4::controls::CyclicPosition{}.WithPosition(1000).WithTorqueOffset(0.12_Nm));
  motor.SetControl(epos4::controls::CyclicVelocity{}.WithVelocity(800).WithVelocityOffset(50));
  motor.SetControl(epos4::controls::CyclicTorque{}.WithTorque(0.2_Nm).WithTorqueOffset(0.05_Nm));

  const auto & mechanism = motor.GetMechanism();
  units::angle::degree_t angle = mechanism.ToAngle(motor.GetCachedPosition());
  auto speed = mechanism.ToAngularVelocity(motor.GetCachedVelocity());
  (void)angle;
  (void)speed;
}

void Homing(epos4::Epos4 & motor)
{
  motor.Home(epos4::controls::Homing{}.WithMethod(
      epos4::signals::HomingMethod::kNegativeLimitSwitch), std::chrono::seconds{30});
  motor.SetPosition(0);
  motor.SetPosition(90_deg);
  std::vector<epos4::signals::HomingMethod> methods;
  motor.GetSupportedHomingMethods(methods);
  epos4::signals::RequiredInput(epos4::signals::HomingMethod::kHomeSwitchPositiveSpeed);
  epos4::signals::RequiresEncoderIndex(epos4::signals::HomingMethod::kIndexPositiveSpeed);
  motor.IsPositionReferenced().GetValueRefreshed();
  motor.IsHomingAttained().Refresh();
  motor.HasHomingError().Refresh();

  epos4::configs::HomingConfigs homing;
  homing.method = epos4::signals::HomingMethod::kNegativeLimitSwitch;
  homing.speedForSwitchSearch = 500;
  homing.speedForZeroSearch = 100;
  homing.acceleration = 2000;
  homing.homeOffsetMoveDistance = 1000;
  homing.homePosition = 0;
  homing.currentThreshold = 1500;
  motor.GetConfigurator().Apply(homing);
}

void Signals(epos4::Epos4 & motor)
{
  auto & position = motor.GetPosition();
  position.Refresh();
  if (!position.GetStatus()) {std::int32_t qc = position.GetValue(); (void)qc;}
  position.GetTimestamp();
  position.GetAge();
  position.HasValue();
  motor.GetPosition().Refresh().IsNear(1000, 50);
  auto & voltage = motor.GetSupplyVoltage();
  auto & temperature = motor.GetPowerStageTemperature();
  auto & current = motor.GetMotorCurrentAveraged();
  epos4::signals::RefreshAll(voltage, temperature, current);
  epos4::signals::IsAllGood(voltage, temperature, current);
  motor.GetMotorCurrent();
  motor.GetPowerStageTemperatureLimit();
  motor.GetMotorI2tPercent();
  motor.GetPowerStageI2tPercent();
  motor.GetPwmDutyCyclePerMille();
  motor.GetStoInputs().Refresh().GetValue().input1Active;
  motor.GetStoCardStatus();
  motor.GetPositionDemand();
  motor.GetVelocityDemand();
  motor.GetVelocityAveraged();
  motor.GetTorque();
  motor.GetTorqueAveraged();
  motor.GetFollowingError();
  motor.GetCurrentDemand();
  motor.GetErrorRegister();
  motor.GetDigitalInputs();
  motor.GetDigitalInputPins();
  motor.GetDigitalOutputs();
  motor.GetDigitalOutputPins();
  motor.GetBrakeState();
  motor.GetCanBitRate();
  motor.GetActiveFieldbus();
  motor.GetSupportedDriveModes();
  motor.IsSetpointAcknowledged();
  motor.IsFollowingCommand();
  motor.IsInternalLimitActive();
  motor.HasWarning();
  motor.IsRemote();
  motor.IsVoltageEnabled();
  motor.GetOperationMode();
  motor.GetStatusword();
}

void Configuration(epos4::Epos4 & motor)
{
  auto & configurator = motor.GetConfigurator();
  epos4::configs::Epos4Configuration cfg;
  cfg.limits.maxMotorSpeed = 4000;
  cfg.velocityControl.p = 150000;
  cfg.velocityControl.i = 20000;
  cfg.motionProfile.profileAcceleration = 5000;
  configurator.Apply(cfg);
  configurator.Refresh(cfg);
  configurator.Save();
  configurator.RestoreDefaults();

  epos4::configs::VelocityControlConfigs gains;
  configurator.Refresh(gains);
  gains.p = *gains.p * 12 / 10;
  configurator.Apply(gains);

  epos4::configs::MotorConfigs motorData;
  motorData.motorType = epos4::signals::MotorType::kBrushlessSinusoidal;
  motorData.nominalCurrent = 9280;
  motorData.outputCurrentLimit = 9280;
  motorData.numberOfPolePairs = 7;
  motorData.torqueConstant = 104790;
  motorData.thermalTimeConstant = 400;
  configurator.Apply(motorData);

  epos4::configs::LimitConfigs limits;
  limits.maxMotorSpeed = 5000;
  limits.minPositionLimit = -200000;
  limits.maxPositionLimit = 200000;
  limits.followingErrorWindow = 2000;
  limits.positionWindow = 20;
  limits.positionWindowTime = 10;
  configurator.Apply(limits);

  epos4::configs::CyclicConfigs cyclic;
  cyclic.interpolationTimePeriodMs = 10;
  configurator.Apply(cyclic);

  epos4::configs::StopOptionConfigs stops;
  stops.shutdown = epos4::signals::ShutdownOption::kSlowDownOnSlowDownRamp;
  stops.faultReaction = epos4::signals::FaultReactionOption::kSlowDownOnQuickStopRamp;
  configurator.Apply(stops);

  epos4::configs::MotionProfileConfigs profile;
  profile.quickStopDeceleration = 10000;
  configurator.Apply(profile);

  epos4::configs::HoldingBrakeConfigs brake;
  brake.couplingTimeMs = 30;
  brake.openingTimeMs = 40;
  configurator.Apply(brake);

  epos4::configs::StandstillConfigs standstill;
  standstill.window = 30;
  standstill.windowTimeMs = 2;
  standstill.windowTimeoutMs = 1000;
  configurator.Apply(standstill);

  epos4::configs::DigitalInputConfigs inputs;
  inputs.input1 = DigitalInputFunction::kNegativeLimitSwitch;
  inputs.input2 = DigitalInputFunction::kPositiveLimitSwitch;
  inputs.input4 = DigitalInputFunction::kGeneralPurposeA;
  inputs.polarity = 0x0000;
  configurator.Apply(inputs);

  epos4::configs::DigitalOutputConfigs outputs;
  outputs.output1 = DigitalOutputFunction::kGeneralPurposeA;
  outputs.output2 = DigitalOutputFunction::kHoldingBrake;
  outputs.polarity = 0x0000;
  configurator.Apply(outputs);

  epos4::configs::AnalogOutputConfigs aout;
  aout.output1 = epos4::signals::AnalogOutputFunction::kGeneralPurposeA;
  configurator.Apply(aout);

  epos4::configs::ProtectionConfigs protection;
  protection.undervoltageLimitMv = 21000;
  protection.overvoltageLimitMv = 30000;
  protection.maxPowerStageTemperatureDeciC = 800;
  configurator.Apply(protection);

  motor.GetBrakeState().Refresh().GetValue();
}

void EncoderAndIo(epos4::Epos4 & motor)
{
  epos4::Encoder & encoder = motor.GetEncoder();
  epos4::configs::SensorsConfigs sensors;
  sensors.sensor1 = epos4::configs::Sensor1Type::kDigitalIncrementalEncoder1;
  sensors.sensor2 = epos4::configs::Sensor2Type::kNone;
  sensors.sensor3 = epos4::configs::Sensor3Type::kDigitalHallSensor;
  encoder.Apply(sensors);
  encoder.Refresh(sensors);

  epos4::configs::DigitalIncrementalEncoderConfigs incremental;
  incremental.encoderNumber = 1;
  incremental.pulsesPerRevolution = 512;
  epos4::configs::IncrementalEncoderType type;
  type.index = epos4::configs::IndexType::kWithIndex;
  type.direction = epos4::configs::EncoderDirection::kMaxon;
  incremental.type = type;
  encoder.Apply(incremental);
  encoder.Refresh(incremental);

  if (encoder.SetMechanismFromDevice(1.0 / 100.0)) {
    encoder.SetMechanism(epos4::Encoder::QuadCountsPerRevolution(512), 1.0 / 100.0);
  }
  if (auto a = encoder.GetAngle()) {units::angle::degree_t deg = *a; (void)deg;}
  encoder.GetAngularVelocity();
  encoder.GetIndexPosition(1);
  encoder.GetHallPattern();
  encoder.GetSsiRawPosition();
  encoder.GetMainSensorResolution();
  encoder.GetSensorPosition(epos4::configs::SensorSlot::kSensor2);
  encoder.GetSensorVelocity(epos4::configs::SensorSlot::kSensor1);
  encoder.GetSensorVelocityAveraged(epos4::configs::SensorSlot::kSensor1);
  epos4::Encoder::SinCosResolution(2048, 4);

  motor.IsNegativeLimitActive();
  motor.IsPositiveLimitActive();
  motor.IsHomeSwitchActive();
  motor.IsInputActive(DigitalInputFunction::kGeneralPurposeA);
  motor.SetDigitalOutput(DigitalOutputFunction::kGeneralPurposeA, true);
  motor.IsOutputActive(DigitalOutputFunction::kGeneralPurposeA);
  auto & v = motor.GetAnalogInputVoltage(epos4::signals::AnalogInput::k1).Refresh();
  (void)v;
  motor.GetAnalogInputGeneralPurpose(epos4::signals::AnalogGeneralPurpose::kA);
  motor.SetAnalogOutput(epos4::signals::AnalogGeneralPurpose::kA, 1.5_V);
  motor.GetAnalogOutputVoltage(epos4::signals::AnalogOutput::k1).Refresh();

  auto probe = epos4::controls::TouchProbe{}
    .WithTrigger(epos4::controls::TouchProbe::Trigger::kInput)
    .WithPositiveEdge(true);
  motor.ArmTouchProbe(probe);
  epos4::signals::TouchProbeState state;
  motor.GetTouchProbe(state);
  if (state.positiveEdgeStored) {std::int32_t where = state.positiveEdgePosition; (void)where;}
  motor.DisarmTouchProbe();
}

void Diagnostics(epos4::Epos4 & motor)
{
  std::printf("%s\n", motor.DescribeLastError().c_str());
  std::printf("%s\n", epos4::signals::DescribeDeviceError(0x8130).c_str());
  std::printf("%s\n", epos4::signals::DeviceErrorName(0x8130).c_str());
  const epos4::signals::DeviceError * e = epos4::signals::FindDeviceError(0x8611);
  (void)e;
  epos4::signals::IsWarning(0x1000);
  epos4::signals::RequiresCommunicationReset(0x8130);
  epos4::signals::DescribeErrorRegister(0x11);
  epos4::signals::DescribeAbortCode(0x06010000);
  epos4::signals::FindAbortCode(0x06010000);
  std::vector<std::uint16_t> history;
  motor.GetErrorHistory(history);
  motor.ClearErrorHistory();
  epos4::signals::DeviceIdentity id;
  motor.GetIdentity(id);
  std::printf("%s %s %s %d\n", epos4::signals::Describe(id).c_str(),
    epos4::signals::FirmwareName(id).c_str(), epos4::signals::HardwareName(0x6552), id.IsMaxon());
  id.HardwareVersion();
  id.SoftwareVersion();
  id.DeviceProfile();
  id.IsProgramValid();
  std::vector<epos4::signals::PdoMismatch> mismatches;
  motor.CheckPdoMapping(mismatches);
  for (const auto & m : mismatches) {std::printf("%s\n", epos4::signals::Describe(m).c_str());}
  epos4::signals::PdoMapping mapping;
  motor.ReadPdoMapping(mapping);
  std::printf("%s", epos4::signals::Describe(mapping).c_str());
  motor.IsPdoActive();
  motor.GetTimeSinceLastPdo();
  std::printf("%s %s\n", epos4::GetVersionString(), epos4::kFirmwareSpecificationEdition);
  epos4::GetVersion();
#if EPOS4_VERSION >= EPOS4_VERSION_ENCODE(0, 1, 0)
#endif
}

void RawAndCore(epos4::Epos4 & motor)
{
  std::int16_t torque{};
  motor.ReadObject(epos4::od::At(epos4::od::cia402::kTorqueActualValue), torque);
  std::uint32_t nominal{};
  motor.ReadObject(epos4::od::maxon::kMotorData_NominalCurrent, nominal);
  std::uint8_t pairs{};
  motor.ReadObject(epos4::od::At(0x3001, 0x03), pairs);
  motor.WriteObject<std::uint32_t>(epos4::od::At(epos4::od::cia402::kProfileAcceleration), 5000);

  std::optional<epos4::signals::State> s = epos4::core::Decode(0x0637);
  epos4::core::Controlword cw;
  cw.Apply(epos4::core::Command::kShutdown);
  cw.Apply(epos4::core::Command::kSwitchOnAndEnableOperation);
  cw.SetModeBits(epos4::signals::control_bits::kNewSetpoint);
  std::uint16_t raw = cw.Raw();
  (void)raw;
  auto step = epos4::core::PlanStep(*s, epos4::core::Goal::kDisabled);
  (void)step.progress;
  (void)step.command;

  std::vector<std::uint8_t> bytes;
  auto writes = epos4::signals::ParseConciseDcf(bytes);
  (void)writes;
  epos4::signals::PdoObject o = epos4::signals::PdoObject::Decode(0x60410010);
  (void)o.bits;

  epos4::MechanismScale scale{2000, 1.0 / 100.0};
  std::int32_t counts{};
  epos4::Resolve(epos4::PositionSetpoint{units::angle::degree_t{90.0}}, scale, counts);

  epos4::configs::LimitConfigs limits;
  limits.maxMotorSpeed = 4000;
  epos4::configs::ConfigWrites cfgWrites;
  limits.AppendTo(cfgWrites);
  epos4::configs::Epos4Configuration all;
  all.ToWrites();
  all.Validate();

  epos4::ToTorque(100, 972400);
  epos4::ToPerThousand(0.1_Nm, 972400);
  epos4::ToCurrent(1500);
  epos4::ToMilliamps(units::current::ampere_t{1.5});
  epos4::ToVoltage(118);
  epos4::ToTemperature(269);
}

int main() {return 0;}
