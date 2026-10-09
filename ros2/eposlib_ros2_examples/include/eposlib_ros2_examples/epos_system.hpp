#pragma once

// A ros2_control hardware interface for joints driven by EPOS4.
//
// Each <joint> of the <ros2_control> block is one drive. Which cyclic mode a
// joint runs in follows from the command interface it declares:
//
//   position  ->  Cyclic Synchronous Position
//   velocity  ->  Cyclic Synchronous Velocity
//   effort    ->  Cyclic Synchronous Torque
//
// read() and write() only use EposLib's lock-free cyclic calls, so the
// controller manager's update loop never waits on the bus. Everything that
// does - booting, enabling, clearing faults - happens in the lifecycle
// transitions.

#include <memory>
#include <string>
#include <vector>

#include <hardware_interface/system_interface.hpp>
#include <hardware_interface/types/hardware_interface_return_values.hpp>
#include <rclcpp/rclcpp.hpp>
#include <rclcpp_lifecycle/state.hpp>

#include "epos4/hardware/Epos4.hpp"

namespace eposlib_ros2_examples
{

class EposSystem : public hardware_interface::SystemInterface
{
public:
  using CallbackReturn = rclcpp_lifecycle::node_interfaces::LifecycleNodeInterface::CallbackReturn;

  CallbackReturn on_init(const hardware_interface::HardwareInfo & info) override;
  CallbackReturn on_configure(const rclcpp_lifecycle::State & previous) override;
  CallbackReturn on_activate(const rclcpp_lifecycle::State & previous) override;
  CallbackReturn on_deactivate(const rclcpp_lifecycle::State & previous) override;
  CallbackReturn on_cleanup(const rclcpp_lifecycle::State & previous) override;

  std::vector<hardware_interface::StateInterface> export_state_interfaces() override;
  std::vector<hardware_interface::CommandInterface> export_command_interfaces() override;

  hardware_interface::return_type read(const rclcpp::Time & time, const rclcpp::Duration & period) override;
  hardware_interface::return_type write(const rclcpp::Time & time, const rclcpp::Duration & period) override;

private:
  enum class Mode {kPosition, kVelocity, kEffort};

  struct Joint
  {
    std::string name;
    std::uint8_t nodeId{0};
    std::uint32_t encoderCounts{2000};  // quadcounts per motor turn
    double gearRatio{1.0};              // output turns per motor turn
    Mode mode{Mode::kPosition};
    double ratedTorqueNm{0.0};          // read at activation, for effort in N m

    // ros2_control's buffers, in SI units at the joint.
    double position{0.0};
    double velocity{0.0};
    double effort{0.0};
    double command{0.0};

    std::unique_ptr<epos4::Epos4> device;
  };

  rclcpp::Logger logger_{rclcpp::get_logger("EposSystem")};
  rclcpp::Clock clock_{RCL_STEADY_TIME};
  std::unique_ptr<epos4::CanBus> bus_;
  std::vector<Joint> joints_;
};

}  // namespace eposlib_ros2_examples
