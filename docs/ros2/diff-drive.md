# Differential drive node

A rover's drive base on two EPOS4 - one per side - as a ROS 2 node: it takes `/cmd_vel`,
turns it into wheel speeds, commands them in Cyclic Synchronous Velocity at 100 Hz, and
publishes the wheels' state and the drives' health.

| Interface | Type | |
|---|---|---|
| `/cmd_vel` | `geometry_msgs/Twist` (sub) | `linear.x` [m/s], `angular.z` [rad/s] |
| `/joint_states` | `sensor_msgs/JointState` (pub) | position [rad] and velocity [rad/s] of each wheel, 100 Hz |
| `/diagnostics` | `diagnostic_msgs/DiagnosticArray` (pub) | per drive: OK / WARN / ERROR, supply, temperature, I²t, last EMCY, 1 Hz |
| `~/enable` | `std_srvs/SetBool` | enable (true) or disable (false) both wheels |
| `~/clear_faults` | `std_srvs/Trigger` | `ClearFault()` on both; then call `~/enable` |

## Running it

```bash
ros2 launch eposlib_ros2_examples diff_drive.launch.py \
  dcf:=$(ros2 pkg prefix eposlib_examples)/share/eposlib_examples/config/two_drives/master.dcf \
  interface:=can0
```

Drive it from the keyboard:

```bash
ros2 run teleop_twist_keyboard teleop_twist_keyboard
```

## Parameters

```yaml title="config/diff_drive.yaml"
--8<-- "eposlib_ros2_examples/config/diff_drive.yaml"
```

The DCF must describe both node-IDs; the two-drive network of the examples
(`examples/config/two_drives/bus.yml`) does.

## The kinematics

For a wheel separation \(L\) and wheel radius \(r\), a body velocity \(v\) (m/s) and yaw rate
\(\omega\) (rad/s) give the wheel angular velocities

\[
\omega_l = \frac{v - \omega L/2}{r},
\qquad
\omega_r = \frac{v + \omega L/2}{r}
\]

in rad/s at the wheel. The node stages them as `units::angular_velocity::radians_per_second_t`;
with `SetMechanism(encoder_counts, gear_ratio)` EposLib converts that to motor rpm:

\[
n_{motor} = \frac{60}{2\pi}\,\frac{\omega_{wheel}}{g}
\]

with \(g\) the gear ratio (output turns per motor turn). A motor mounted mirrored on the right
side turns the other way for the same direction of travel: `right_inverted` flips its sign,
in both the command and `/joint_states`.

## How it is built

**Two callback groups, two threads.** The 100 Hz control timer and the `/cmd_vel`
subscription are in the default group and use only lock-free calls - `StageTargetVelocity()`,
`GetCached*()`, `IsCyclicHealthy()` - so they never wait on the bus. Diagnostics and the two
services do block (SDO reads, `Enable()`, `ClearFault()`), so they live in a second
mutually-exclusive group, served by the other thread of a `MultiThreadedExecutor`.

**A command timeout.** If no `/cmd_vel` arrives for `cmd_timeout_ms`, the wheels are
commanded to zero. A dropped joystick or a crashed planner must not leave the rover driving.

**Faults at start-up.** If a drive is in Fault when the node starts - the `0x8130` heartbeat
fault every program exit leaves behind - it is logged and cleared before enabling.

**Destruction order.** The bus is the first member, so it is destroyed last; the destructor
stages zero, leaves the cyclic mode and disables both wheels before stopping the bus.

## Source

```cpp title="src/diff_drive_node.cpp"
--8<-- "eposlib_ros2_examples/src/diff_drive_node.cpp"
```
