#include "eposlib_ros2_examples/epos_system.hpp"

#include <chrono>
#include <cmath>
#include <limits>

#include <hardware_interface/types/hardware_interface_type_values.hpp>
#include <pluginlib/class_list_macros.hpp>
#include <ros2units/units.h>

namespace eposlib_ros2_examples
{

namespace
{

std::string Param(
  const std::unordered_map<std::string, std::string> & params, const std::string & key,
  const std::string & fallback)
{
  const auto it = params.find(key);
  return it == params.end() ? fallback : it->second;
}

}  // namespace

EposSystem::CallbackReturn EposSystem::on_init(const hardware_interface::HardwareInfo & info)
{
  if (hardware_interface::SystemInterface::on_init(info) != CallbackReturn::SUCCESS) {
    return CallbackReturn::ERROR;
  }

  // <hardware> parameters: the CAN network.
  if (Param(info_.hardware_parameters, "dcf", "").empty()) {
    RCLCPP_FATAL(logger_, "<param name=\"dcf\"> is required");
    return CallbackReturn::ERROR;
  }

  // <joint> parameters: one drive each.
  for (const auto & j : info_.joints) {
    Joint joint;
    joint.name = j.name;
    joint.nodeId = static_cast<std::uint8_t>(std::stoi(Param(j.parameters, "node_id", "0")));
    joint.encoderCounts = static_cast<std::uint32_t>(std::stoul(Param(j.parameters, "encoder_counts", "2000")));
    joint.gearRatio = std::stod(Param(j.parameters, "gear_ratio", "1.0"));
    if (joint.nodeId == 0 || j.command_interfaces.size() != 1) {
      RCLCPP_FATAL(logger_, "joint %s: needs node_id and exactly one command interface", j.name.c_str());
      return CallbackReturn::ERROR;
    }
    const auto & cmd = j.command_interfaces[0].name;
    if (cmd == hardware_interface::HW_IF_POSITION) {joint.mode = Mode::kPosition;}
    else if (cmd == hardware_interface::HW_IF_VELOCITY) {joint.mode = Mode::kVelocity;}
    else if (cmd == hardware_interface::HW_IF_EFFORT) {joint.mode = Mode::kEffort;}
    else {
      RCLCPP_FATAL(logger_, "joint %s: unsupported command interface %s", j.name.c_str(), cmd.c_str());
      return CallbackReturn::ERROR;
    }
    joints_.push_back(std::move(joint));
  }
  return CallbackReturn::SUCCESS;
}

EposSystem::CallbackReturn EposSystem::on_configure(const rclcpp_lifecycle::State &)
{
  epos4::CanBus::Options options;
  options.interface = Param(info_.hardware_parameters, "interface", "can0");
  options.masterDcf = Param(info_.hardware_parameters, "dcf", "");
  options.masterNodeId = static_cast<std::uint8_t>(
    std::stoi(Param(info_.hardware_parameters, "master_node_id", "1")));
  bus_ = std::make_unique<epos4::CanBus>(options);

  // Every device before Start(), so the master routes their PDOs.
  for (auto & joint : joints_) {
    joint.device = std::make_unique<epos4::Epos4>(*bus_, joint.nodeId);
    joint.device->SetMechanism(joint.encoderCounts, joint.gearRatio);
  }
  try {
    bus_->Start();
  } catch (const std::exception & e) {
    RCLCPP_FATAL(logger_, "cannot start the CAN bus: %s", e.what());
    return CallbackReturn::ERROR;
  }
  for (auto & joint : joints_) {
    if (!joint.device->WaitUntilReady()) {
      RCLCPP_FATAL(logger_, "joint %s: node %u does not answer", joint.name.c_str(), joint.nodeId);
      return CallbackReturn::ERROR;
    }
    const auto boot = joint.device->GetBootStatus();
    if (boot.count > 0 && !boot.Succeeded()) {
      RCLCPP_ERROR(logger_, "joint %s: boot '%c' %s", joint.name.c_str(), boot.errorStatus, boot.what.c_str());
    }
  }
  return CallbackReturn::SUCCESS;
}

EposSystem::CallbackReturn EposSystem::on_activate(const rclcpp_lifecycle::State &)
{
  for (auto & joint : joints_) {
    auto & d = *joint.device;
    if (d.IsFaulted()) {
      RCLCPP_WARN(logger_, "joint %s: %s", joint.name.c_str(), d.DescribeLastError().c_str());
      d.ClearFault();
    }
    if (joint.mode == Mode::kEffort) {
      auto & rated = d.GetMotorRatedTorque().Refresh();
      if (rated.GetStatus() || rated.GetValue() == 0) {
        RCLCPP_FATAL(logger_, "joint %s: rated torque is 0 - configure the motor data", joint.name.c_str());
        return CallbackReturn::ERROR;
      }
      joint.ratedTorqueNm = rated.GetValue() / 1e6;
    }
    if (!d.Enable()) {
      RCLCPP_FATAL(logger_, "joint %s: cannot enable", joint.name.c_str());
      return CallbackReturn::ERROR;
    }
    std::error_code ec;
    switch (joint.mode) {
      case Mode::kPosition: ec = d.EnterCyclicPositionMode(); break;
      case Mode::kVelocity: ec = d.EnterCyclicVelocityMode(); break;
      case Mode::kEffort: ec = d.EnterCyclicTorqueMode(); break;
    }
    if (ec) {
      RCLCPP_FATAL(logger_, "joint %s: cannot enter the cyclic mode: %s", joint.name.c_str(), ec.message().c_str());
      return CallbackReturn::ERROR;
    }
  }
  // Start every command where the joint is: no jump on the first cycle.
  read(rclcpp::Time{}, rclcpp::Duration::from_seconds(0));
  for (auto & joint : joints_) {
    joint.command = joint.mode == Mode::kPosition ? joint.position : 0.0;
  }
  return CallbackReturn::SUCCESS;
}

EposSystem::CallbackReturn EposSystem::on_deactivate(const rclcpp_lifecycle::State &)
{
  for (auto & joint : joints_) {
    joint.device->ExitCyclicMode();
    joint.device->Disable();
  }
  return CallbackReturn::SUCCESS;
}

EposSystem::CallbackReturn EposSystem::on_cleanup(const rclcpp_lifecycle::State &)
{
  // Devices before the bus.
  for (auto & joint : joints_) {joint.device.reset();}
  bus_.reset();
  return CallbackReturn::SUCCESS;
}

std::vector<hardware_interface::StateInterface> EposSystem::export_state_interfaces()
{
  std::vector<hardware_interface::StateInterface> out;
  for (auto & joint : joints_) {
    out.emplace_back(joint.name, hardware_interface::HW_IF_POSITION, &joint.position);
    out.emplace_back(joint.name, hardware_interface::HW_IF_VELOCITY, &joint.velocity);
    out.emplace_back(joint.name, hardware_interface::HW_IF_EFFORT, &joint.effort);
  }
  return out;
}

std::vector<hardware_interface::CommandInterface> EposSystem::export_command_interfaces()
{
  std::vector<hardware_interface::CommandInterface> out;
  for (auto & joint : joints_) {
    const char * kind = joint.mode == Mode::kPosition ? hardware_interface::HW_IF_POSITION :
      joint.mode == Mode::kVelocity ? hardware_interface::HW_IF_VELOCITY :
      hardware_interface::HW_IF_EFFORT;
    out.emplace_back(joint.name, kind, &joint.command);
  }
  return out;
}

hardware_interface::return_type EposSystem::read(const rclcpp::Time &, const rclcpp::Duration &)
{
  for (auto & joint : joints_) {
    const auto & d = *joint.device;
    const auto & m = d.GetMechanism();
    // Lock-free: the values of the last TPDO, converted to the joint.
    const units::angle::radian_t angle = m.ToAngle(d.GetCachedPosition());
    const units::angular_velocity::radians_per_second_t speed = m.ToAngularVelocity(d.GetCachedVelocity());
    joint.position = angle.value();
    joint.velocity = speed.value();
    // Motor torque, through the gearbox: output torque = motor torque / ratio.
    joint.effort = d.GetCachedTorque() / 1000.0 * joint.ratedTorqueNm / joint.gearRatio;
  }
  return hardware_interface::return_type::OK;
}

hardware_interface::return_type EposSystem::write(const rclcpp::Time &, const rclcpp::Duration &)
{
  bool healthy = true;
  for (auto & joint : joints_) {
    auto & d = *joint.device;
    if (std::isnan(joint.command)) {continue;}
    switch (joint.mode) {
      case Mode::kPosition:
        d.StageTargetPosition(units::angle::radian_t{joint.command});
        break;
      case Mode::kVelocity:
        d.StageTargetVelocity(units::angular_velocity::radians_per_second_t{joint.command});
        break;
      case Mode::kEffort:
        // N m at the joint -> N m at the motor shaft.
        d.StageTargetTorque(units::torque::newton_meter_t{joint.command * joint.gearRatio});
        break;
    }
    healthy = d.IsCyclicHealthy() && healthy;
  }
  if (!healthy) {
    RCLCPP_ERROR_THROTTLE(logger_, clock_, 2000, "a joint is not healthy");
  }
  return hardware_interface::return_type::OK;
}

}  // namespace eposlib_ros2_examples

PLUGINLIB_EXPORT_CLASS(eposlib_ros2_examples::EposSystem, hardware_interface::SystemInterface)
