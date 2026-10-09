# Patterns for ROS 2 nodes

How to fit EposLib into the ROS 2 execution model without stalling a callback on the bus.

## Separate the fast path from the slow one

EposLib has two kinds of call ([Threading and timing](../concepts/threading.md)): lock-free
ones for the cyclic path, and blocking ones for everything else. In a ROS 2 node, map them to
**two callback groups**:

```cpp
// Fast: lock-free calls only. Default (mutually exclusive) group.
control_ = create_wall_timer(10ms, [this] {
  motor_.StageTargetVelocity(target_);           // atomic store
  publishState(motor_.GetCachedVelocity());       // atomic load
});

// Slow: anything that waits on the bus. Its own group.
slow_ = create_callback_group(rclcpp::CallbackGroupType::MutuallyExclusive);
diag_ = create_wall_timer(1s, [this] {
  epos4::signals::RefreshAll(motor_.GetSupplyVoltage(), motor_.GetPowerStageTemperature());
}, slow_);
```

and spin with a **multi-threaded executor**, so the slow group runs on another thread:

```cpp
rclcpp::executors::MultiThreadedExecutor executor(rclcpp::ExecutorOptions(), 2);
executor.add_node(node);
executor.spin();
```

With a single-threaded executor a 3 ms SDO read in a diagnostics timer delays the control
timer by 3 ms - and a `ClearFault()` in a service by up to three seconds.

## Construct in the right order

```cpp
class MyNode : public rclcpp::Node
{
  epos4::CanBus bus_;     // first: constructed first, destroyed last
  epos4::Epos4 left_;
  epos4::Epos4 right_;
};
```

and call `bus_.Start()` in the constructor **after** the devices exist (member initialisers
run in declaration order, before the constructor body). The destructor runs before the
members are destroyed - disable there:

```cpp
~MyNode() override
{
  for (auto * m : {&left_, &right_}) {m->ExitCyclicMode(); m->Disable();}
  bus_.Stop();
}
```

A `CanBus` that fails to start throws `std::system_error` from the constructor; let it
propagate, or catch it and fail the node's startup with a clear message.

## Parameters

Bus and device configuration are natural parameters: interface, DCF path, node-IDs,
mechanism. Parameters must be declared before the members that use them are constructed -
in the member initialiser list, through a helper:

```cpp
epos4::CanBus::Options BusOptions(rclcpp::Node & node)
{
  epos4::CanBus::Options o;
  o.interface = node.declare_parameter<std::string>("interface", "can0");
  o.masterDcf = node.declare_parameter<std::string>("dcf", "");
  return o;
}

MyNode() : Node("my_node"), bus_(BusOptions(*this)),
           motor_(bus_, static_cast<std::uint8_t>(declare_parameter<int>("node_id", 2))) {}
```

Find an installed DCF with `ament_index_cpp::get_package_share_directory()`.

## Lifecycle nodes

A `rclcpp_lifecycle::LifecycleNode` maps onto the drive's own lifecycle:

| Lifecycle transition | EposLib |
|---|---|
| `on_configure` | create `CanBus` and the devices, `Start()`, `WaitUntilReady()`, check boot status and PDO mapping, apply configuration |
| `on_activate` | `ClearFault()` if needed, `Enable()`, `Enter*Mode()` |
| `on_deactivate` | `ExitCyclicMode()`, `Disable()` |
| `on_cleanup` | destroy the devices, then the bus |
| `on_shutdown` | as deactivate + cleanup |

The [ros2_control hardware interface](ros2-control.md) follows exactly this mapping.

## Diagnostics

Publish `diagnostic_msgs/DiagnosticArray` at 1 Hz from the slow group - supply voltage,
temperature, I²t, the last EMCY, and the state - with ERROR for a fault and WARN for an axis
that is not healthy. `rqt_robot_monitor` and the diagnostic aggregator then show the drives
next to the rest of the robot. The [differential drive node](diff-drive.md) has a complete
implementation.

## Faults as events

The emergency callback runs on the CANopen thread. In a node, keep it to setting a flag or
pushing to a queue, and act from a timer in the slow group:

```cpp
motor_.SetEmergencyCallback([this](const epos4::signals::EmergencyMessage & m) {
  if (m.errorCode != 0 && !epos4::signals::IsWarning(m.errorCode)) {
    faulted_ = true;                       // std::atomic<bool>
  }
});
```

Never call `rclcpp` publishers or EposLib's blocking calls from inside it.

## Time

ROS time and EposLib time are different clocks. EposLib's ages - `GetTimeSinceLastPdo()`,
`GetAge()` - are `steady_clock` durations, unaffected by simulated time; stamp messages with
`now()` as usual.
