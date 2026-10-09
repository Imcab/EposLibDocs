// diff_drive_node: a differential-drive base on two EPOS4, for a rover.
//
// Subscribes  /cmd_vel           geometry_msgs/Twist
// Publishes   /joint_states      sensor_msgs/JointState     100 Hz
//             /diagnostics       diagnostic_msgs/DiagnosticArray   1 Hz
// Services    ~/enable           std_srvs/SetBool
//             ~/clear_faults     std_srvs/Trigger
//
//   ros2 run eposlib_ros2_examples diff_drive_node --ros-args
//     -p dcf:=/path/to/master.dcf -p interface:=can0
//     -p left_node_id:=2 -p right_node_id:=3
//
// Two rules shape it. The 100 Hz control timer only uses EposLib's lock-free
// calls (StageTargetVelocity, GetCached*, IsCyclicHealthy), so it never waits
// on the bus. Everything that blocks - diagnostics, enabling, clearing faults
// - runs in a second callback group, on another thread of a multi-threaded
// executor, so it can never delay the control timer.

#include <algorithm>
#include <cstdio>
#include <chrono>
#include <cmath>
#include <memory>
#include <mutex>
#include <string>

#include <diagnostic_msgs/msg/diagnostic_array.hpp>
#include <geometry_msgs/msg/twist.hpp>
#include <rclcpp/rclcpp.hpp>
#include <ros2units/units.h>
#include <sensor_msgs/msg/joint_state.hpp>
#include <std_srvs/srv/set_bool.hpp>
#include <std_srvs/srv/trigger.hpp>

#include "epos4/hardware/Epos4.hpp"

using namespace std::chrono_literals;

namespace
{

epos4::CanBus::Options BusOptions(rclcpp::Node & node)
{
  epos4::CanBus::Options options;
  options.interface = node.declare_parameter<std::string>("interface", "can0");
  options.masterDcf = node.declare_parameter<std::string>("dcf", "");
  options.masterNodeId = static_cast<std::uint8_t>(node.declare_parameter<int>("master_node_id", 1));
  return options;
}

}  // namespace

class DiffDriveNode : public rclcpp::Node
{
public:
  DiffDriveNode()
  : Node("diff_drive"),
    bus_(BusOptions(*this)),
    left_(bus_, static_cast<std::uint8_t>(declare_parameter<int>("left_node_id", 2))),
    right_(bus_, static_cast<std::uint8_t>(declare_parameter<int>("right_node_id", 3)))
  {
    wheelRadius_ = declare_parameter<double>("wheel_radius", 0.12);          // m
    wheelSeparation_ = declare_parameter<double>("wheel_separation", 0.55);  // m
    const auto counts = declare_parameter<int>("encoder_counts", 2000);     // qc per motor turn
    const auto gear = declare_parameter<double>("gear_ratio", 1.0 / 100.0); // output / motor turns
    rightInverted_ = declare_parameter<bool>("right_inverted", true);       // mirrored mounting
    maxWheelRpm_ = declare_parameter<double>("max_wheel_rpm", 60.0);        // at the wheel
    cmdTimeout_ = std::chrono::milliseconds(declare_parameter<int>("cmd_timeout_ms", 500));

    // The mechanism lets the wheels be commanded and read at the output.
    for (auto * wheel : {&left_, &right_}) {
      wheel->SetMechanism(static_cast<std::uint32_t>(counts), gear);
    }

    // Every device exists: now the master may boot the network.
    bus_.Start();
    for (auto * wheel : {&left_, &right_}) {
      if (!wheel->WaitUntilReady()) {
        throw std::runtime_error("node " + std::to_string(wheel->GetNodeId()) + " does not answer");
      }
    }
    EnableWheels();

    jointPub_ = create_publisher<sensor_msgs::msg::JointState>("joint_states", 10);
    diagPub_ = create_publisher<diagnostic_msgs::msg::DiagnosticArray>("diagnostics", 10);
    cmdSub_ = create_subscription<geometry_msgs::msg::Twist>(
      "cmd_vel", 10, [this](geometry_msgs::msg::Twist::ConstSharedPtr msg) {OnCmdVel(*msg);});

    // Control: lock-free only, in the default callback group.
    controlTimer_ = create_wall_timer(10ms, [this] {Control();});

    // Anything that blocks on the bus: its own group, its own thread.
    slow_ = create_callback_group(rclcpp::CallbackGroupType::MutuallyExclusive);
    diagTimer_ = create_wall_timer(1s, [this] {PublishDiagnostics();}, slow_);
    enableSrv_ = create_service<std_srvs::srv::SetBool>(
      "~/enable",
      [this](std_srvs::srv::SetBool::Request::SharedPtr req,
      std_srvs::srv::SetBool::Response::SharedPtr res) {
        res->success = req->data ? EnableWheels() : DisableWheels();
        res->message = res->success ? "ok" : "failed - see /diagnostics";
      },
      rmw_qos_profile_services_default, slow_);
    clearSrv_ = create_service<std_srvs::srv::Trigger>(
      "~/clear_faults",
      [this](std_srvs::srv::Trigger::Request::SharedPtr,
      std_srvs::srv::Trigger::Response::SharedPtr res) {
        res->success = left_.ClearFault() && right_.ClearFault();
        res->message = res->success ? "faults cleared - call ~/enable" : "still faulted";
      },
      rmw_qos_profile_services_default, slow_);

    RCLCPP_INFO(get_logger(), "wheels on nodes %u and %u ready", left_.GetNodeId(), right_.GetNodeId());
  }

  ~DiffDriveNode() override
  {
    DisableWheels();
    bus_.Stop();
  }

private:
  bool EnableWheels()
  {
    for (auto * wheel : {&left_, &right_}) {
      if (wheel->IsFaulted()) {
        RCLCPP_WARN(get_logger(), "node %u: %s", wheel->GetNodeId(), wheel->DescribeLastError().c_str());
        wheel->ClearFault();   // e.g. the heartbeat fault every program start leaves behind
      }
      if (!wheel->Enable() || wheel->EnterCyclicVelocityMode()) {
        RCLCPP_ERROR(get_logger(), "node %u cannot be enabled", wheel->GetNodeId());
        return false;
      }
    }
    return true;
  }

  bool DisableWheels()
  {
    bool ok = true;
    for (auto * wheel : {&left_, &right_}) {
      wheel->StageTargetVelocity(0);
      wheel->ExitCyclicMode();
      ok = wheel->Disable() && ok;
    }
    return ok;
  }

  void OnCmdVel(const geometry_msgs::msg::Twist & msg)
  {
    // Differential drive kinematics: wheel angular speed in rad/s.
    const double v = msg.linear.x;
    const double w = msg.angular.z;
    std::lock_guard<std::mutex> lock{cmdMutex_};
    leftRadS_ = (v - w * wheelSeparation_ / 2.0) / wheelRadius_;
    rightRadS_ = (v + w * wheelSeparation_ / 2.0) / wheelRadius_;
    lastCmd_ = now();
  }

  void Control()
  {
    double l = 0.0;
    double r = 0.0;
    {
      std::lock_guard<std::mutex> lock{cmdMutex_};
      // No command for a while: stop. A lost joystick must not leave the
      // rover driving.
      if ((now() - lastCmd_) < rclcpp::Duration(cmdTimeout_)) {
        l = leftRadS_;
        r = rightRadS_;
      }
    }
    const double limit = maxWheelRpm_ * 2.0 * M_PI / 60.0;
    l = std::clamp(l, -limit, limit);
    r = std::clamp(r, -limit, limit);
    if (rightInverted_) {r = -r;}

    // rad/s at the wheel -> a quantity; EposLib converts it to motor rpm
    // through the mechanism. Lock-free: an atomic store.
    using units::angular_velocity::radians_per_second_t;
    left_.StageTargetVelocity(radians_per_second_t{l});
    right_.StageTargetVelocity(radians_per_second_t{r});

    if (!left_.IsCyclicHealthy() || !right_.IsCyclicHealthy()) {
      RCLCPP_WARN_THROTTLE(get_logger(), *get_clock(), 2000, "a wheel is not healthy");
    }

    sensor_msgs::msg::JointState js;
    js.header.stamp = now();
    js.name = {"left_wheel_joint", "right_wheel_joint"};
    const double sign[2] = {1.0, rightInverted_ ? -1.0 : 1.0};
    int k = 0;
    for (auto * wheel : {&left_, &right_}) {
      const auto & m = wheel->GetMechanism();
      const units::angle::radian_t angle = m.ToAngle(wheel->GetCachedPosition());
      const units::angular_velocity::radians_per_second_t speed =
        m.ToAngularVelocity(wheel->GetCachedVelocity());
      js.position.push_back(sign[k] * angle.value());
      js.velocity.push_back(sign[k] * speed.value());
      ++k;
    }
    jointPub_->publish(js);
  }

  void PublishDiagnostics()
  {
    diagnostic_msgs::msg::DiagnosticArray array;
    array.header.stamp = now();
    for (auto * wheel : {&left_, &right_}) {
      auto & supply = wheel->GetSupplyVoltage();
      auto & temp = wheel->GetPowerStageTemperature();
      auto & i2t = wheel->GetMotorI2tPercent();
      epos4::signals::RefreshAll(supply, temp, i2t);

      diagnostic_msgs::msg::DiagnosticStatus st;
      st.name = "epos4/node_" + std::to_string(wheel->GetNodeId());
      st.hardware_id = st.name;
      const bool faulted = wheel->IsFaulted();
      st.level = faulted ? diagnostic_msgs::msg::DiagnosticStatus::ERROR :
        (!wheel->IsCyclicHealthy() ? diagnostic_msgs::msg::DiagnosticStatus::WARN :
        diagnostic_msgs::msg::DiagnosticStatus::OK);
      st.message = faulted ? epos4::signals::DeviceErrorName(wheel->GetErrorCode().Refresh().GetValue()) :
        (wheel->IsCyclicHealthy() ? "ok" : "no PDOs or not enabled");
      auto kv = [](const std::string & k, const std::string & v) {
          diagnostic_msgs::msg::KeyValue p; p.key = k; p.value = v; return p;
        };
      st.values.push_back(kv("supply_V", std::to_string(supply.GetValue().value())));
      st.values.push_back(kv("power_stage_degC", std::to_string(temp.GetValue().value())));
      st.values.push_back(kv("i2t_motor_pct", std::to_string(i2t.GetValue())));
      char emcy[8];
      std::snprintf(emcy, sizeof(emcy), "0x%04X", wheel->GetCachedErrorCode());
      st.values.push_back(kv("last_emcy", emcy));
      array.status.push_back(st);
    }
    diagPub_->publish(array);
  }

  // The bus first: members are destroyed in reverse order, devices before it.
  epos4::CanBus bus_;
  epos4::Epos4 left_;
  epos4::Epos4 right_;

  double wheelRadius_{0.12};
  double wheelSeparation_{0.55};
  bool rightInverted_{true};
  double maxWheelRpm_{60.0};
  std::chrono::milliseconds cmdTimeout_{500};

  std::mutex cmdMutex_;
  double leftRadS_{0.0};
  double rightRadS_{0.0};
  rclcpp::Time lastCmd_{0, 0, RCL_ROS_TIME};

  rclcpp::CallbackGroup::SharedPtr slow_;
  rclcpp::Publisher<sensor_msgs::msg::JointState>::SharedPtr jointPub_;
  rclcpp::Publisher<diagnostic_msgs::msg::DiagnosticArray>::SharedPtr diagPub_;
  rclcpp::Subscription<geometry_msgs::msg::Twist>::SharedPtr cmdSub_;
  rclcpp::TimerBase::SharedPtr controlTimer_;
  rclcpp::TimerBase::SharedPtr diagTimer_;
  rclcpp::Service<std_srvs::srv::SetBool>::SharedPtr enableSrv_;
  rclcpp::Service<std_srvs::srv::Trigger>::SharedPtr clearSrv_;
};

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  auto node = std::make_shared<DiffDriveNode>();
  // Two threads: one for the control timer and cmd_vel, one for the slow group.
  rclcpp::executors::MultiThreadedExecutor executor(rclcpp::ExecutorOptions(), 2);
  executor.add_node(node);
  executor.spin();
  rclcpp::shutdown();
  return 0;
}
