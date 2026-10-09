# Using EposLib in your package

A robot's own package links the library and carries its own **network description**: which
drives are on the bus, at which node-IDs, with which PDO mapping.

## Layout

```text
my_robot_drives/
├── CMakeLists.txt
├── package.xml
├── config/
│   └── arm_bus/
│       ├── bus.yml        # the network: master + one block per drive
│       └── epos4.eds      # the drive's EDS (copy it from EposLib)
└── src/
    └── arm_node.cpp
```

## package.xml

```xml
<buildtool_depend>cmake</buildtool_depend>   <!-- or ament_cmake -->
<depend>eposlib</depend>
<depend>ros2units</depend>
```

## CMakeLists.txt

```cmake
cmake_minimum_required(VERSION 3.18)
project(my_robot_drives LANGUAGES CXX)

find_package(eposlib 0.1 REQUIRED)

# config/arm_bus/bus.yml + the EDS next to it -> master.dcf and one node_N.bin
# per drive, regenerated whenever bus.yml or an EDS changes, and installed to
# share/my_robot_drives/config/arm_bus/.
eposlib_generate_dcf(arm_bus DESTINATION share/${PROJECT_NAME}/config)

add_executable(arm_node src/arm_node.cpp)
target_link_libraries(arm_node PRIVATE eposlib::eposlib)
install(TARGETS arm_node DESTINATION lib/${PROJECT_NAME})
```

`find_package(eposlib)` gives:

| | |
|---|---|
| `eposlib::eposlib` | the device API: `CanBus`, `Epos4`, `Encoder`. Link this. |
| `eposlib::core` | the pure CiA 402 logic - state machine, units, configs, error tables - without Lely. See [The core library](../api/core.md). |
| `eposlib_generate_dcf()` | the network description to master DCF, at build time |

The version is compatible within the same minor version (`SameMinorVersion`): before 1.0 a
minor version may change the API.

### eposlib_generate_dcf

```cmake
eposlib_generate_dcf(<network> [DESTINATION <dir>] [SOURCE_DIR <dir>])
```

- Reads `config/<network>/bus.yml` (or `SOURCE_DIR`) and the `.eds` files next to it.
- Runs `dcfgen` into `${CMAKE_CURRENT_BINARY_DIR}/config/<network>`.
- With `DESTINATION`, installs the sources and the generated files to `<DESTINATION>/<network>`.
- If `dcfgen` is not found it warns and generates nothing; the build still succeeds, but
  there is no DCF to run with.

It is the plain-CMake equivalent of the `generate_dcf()` macro of `lely_core_libraries`, so
the same package builds with or without ROS.

## Finding the DCF at run time

The installed DCF is at `<prefix>/share/<package>/config/<network>/master.dcf`. Two common
ways to hand it to `CanBus`:

```cmake
# Bake the install path in at configure time.
target_compile_definitions(arm_node PRIVATE
  ARM_DCF="${CMAKE_INSTALL_PREFIX}/share/${PROJECT_NAME}/config/arm_bus/master.dcf")
```

```cpp
// Or, in a ROS 2 node, ask ament_index_cpp.
#include <ament_index_cpp/get_package_share_directory.hpp>
const auto dcf = ament_index_cpp::get_package_share_directory("my_robot_drives") +
                 "/config/arm_bus/master.dcf";
```

The DCF names each node's concise configuration (`node_N.bin`) with a relative path.
`CanBus::Start()` rewrites those paths against the DCF's own directory, into a
`master.resolved.dcf` next to it, so the program can run from any working directory.

!!! warning "The install directory must be writable"
    `master.resolved.dcf` is written next to `master.dcf`. If that directory is read-only,
    `CanBus` falls back to the original DCF, and the node configuration is then looked up
    relative to the **working directory** - run from anywhere else and the master's
    configuration download fails with `es='J'`.

## In a ROS 2 node

EposLib has no ROS dependency, so it fits any node structure. The usual shape:

```cpp
class ArmNode : public rclcpp::Node
{
public:
  ArmNode()
  : Node("arm"), bus_({"can0", Dcf(), 1}), shoulder_(bus_, 2), elbow_(bus_, 3)
  {
    bus_.Start();                                  // after the devices exist
    // ... WaitUntilReady, Enable, EnterCyclicPositionMode ...
    timer_ = create_wall_timer(std::chrono::milliseconds(10), [this] {Step();});
  }

private:
  void Step()
  {
    // Lock-free: never blocks the executor.
    shoulder_.StageTargetPosition(shoulderTarget_);
    elbow_.StageTargetPosition(elbowTarget_);
  }

  epos4::CanBus bus_;        // declared first, destroyed last
  epos4::Epos4 shoulder_;
  epos4::Epos4 elbow_;
  // ...
};
```

Declare the bus **before** the devices: members are destroyed in reverse order, so the
devices go first, as they must. Keep blocking calls (`Enable()`, `Home()`, configuration)
out of callbacks that must stay fast - see [Threading and timing](../concepts/threading.md).
