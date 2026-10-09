#!/usr/bin/env bash
# Builds eposlib_ros2_examples and runs both of its programs against
# epos4_sim on a vcan0 inside a throwaway container.
#
#   scripts/check_ros2.sh [path/to/EposLib] [path/to/ros2units]
#
# Needs an image with ROS 2 Humble, Lely, ros2_control and xacro
# (IMAGE, default ros2_humble_gazebo).
set -euo pipefail

DOCS="$(cd "$(dirname "$0")/.." && pwd)"
EPOSLIB="$(cd "${1:-$HOME/ros2_ws/src/EposLib}" && pwd)"
UNITS="$(cd "${2:-$HOME/ros2_ws/src/ros2units}" && pwd)"
IMAGE="${IMAGE:-ros2_humble_gazebo}"

docker run --rm --privileged --user root \
  -v "$EPOSLIB":/src/EposLib:ro -v "$UNITS":/src/ros2units:ro \
  -v "$DOCS/examples":/src/eposlib_examples:ro \
  -v "$DOCS/ros2/eposlib_ros2_examples":/src/eposlib_ros2_examples:ro \
  "$IMAGE" bash -c '
set -o pipefail
ip link add dev vcan0 type vcan && ip link set up vcan0
mkdir -p /ws/src && cp -r /src/* /ws/src/ && cd /ws && source /opt/ros/humble/setup.bash
if ! colcon build --cmake-args -DBUILD_TESTING=OFF > build.log 2>&1; then tail -60 build.log; exit 1; fi
if grep -E "warning:" build.log; then echo "BUILD WARNINGS"; exit 1; fi
echo "build: eposlib_ros2_examples compiled without warnings"
source install/setup.bash
export ROS_DOMAIN_ID=77 RCUTILS_COLORIZED_OUTPUT=0
SIM=/ws/install/eposlib/lib/eposlib/epos4_sim
CFG=/ws/install/eposlib/share/eposlib/config/epos4_network
TWO=/ws/install/eposlib_examples/share/eposlib_examples/config/two_drives
failures=0

ros2 daemon start > /dev/null 2>&1
echo "[$(date +%T)] ===== diff_drive_node"
$SIM $CFG/epos4.eds 2 vcan0 > /ws/sim2.log 2>&1 & s2=$!
$SIM $CFG/epos4.eds 3 vcan0 > /ws/sim3.log 2>&1 & s3=$!
sleep 0.5
ros2 run eposlib_ros2_examples diff_drive_node --ros-args -p dcf:=$TWO/master.dcf -p interface:=vcan0 > /ws/dd.log 2>&1 & dd=$!
for i in $(seq 1 40); do grep -q "ready" /ws/dd.log && break; sleep 0.25; done
grep "ready" /ws/dd.log || { cat /ws/dd.log; failures=$((failures+1)); }
timeout -s KILL 6 ros2 topic pub -r 20 /cmd_vel geometry_msgs/msg/Twist "{linear: {x: 0.5}, angular: {z: 0.4}}" > /dev/null 2>&1 &
sleep 4
echo "[$(date +%T)] reading joint_states"
js=$(timeout -s KILL 8 ros2 topic echo --once /joint_states sensor_msgs/msg/JointState 2>/dev/null)
echo "$js" | sed -n "/^name/,/^effort/p"
if echo "$js" | grep -A2 "^velocity:" | grep -qE "[1-9]"; then echo "PASS  wheels turn on cmd_vel"; else echo "FAIL  wheels"; failures=$((failures+1)); fi
timeout -s KILL 8 ros2 topic echo --once /diagnostics diagnostic_msgs/msg/DiagnosticArray 2>/dev/null | grep -E "name:|message:|key:|value:" | head -12
timeout -s KILL 10 ros2 service call /diff_drive/clear_faults std_srvs/srv/Trigger 2>/dev/null | tail -1
sleep 2.5
js2=$(timeout -s KILL 8 ros2 topic echo --once /joint_states sensor_msgs/msg/JointState 2>/dev/null)
echo "after the cmd_vel timeout:"; echo "$js2" | grep -A2 "^velocity:"
echo "[$(date +%T)] stopping diff_drive_node"
pkill -INT -x diff_drive_node
for i in $(seq 1 20); do pgrep -x diff_drive_node > /dev/null || break; sleep 0.25; done
if pgrep -x diff_drive_node > /dev/null; then echo "FAIL  diff_drive_node did not exit on SIGINT"; failures=$((failures+1)); pkill -KILL -x diff_drive_node; else echo "PASS  diff_drive_node exits on SIGINT"; fi
tail -3 /ws/dd.log
kill $s2 $s3; wait $s2 $s3 2>/dev/null

echo "[$(date +%T)] ===== ros2_control EposSystem"
$SIM $CFG/epos4.eds 2 vcan0 > /ws/sim.log 2>&1 & s=$!
sleep 0.5
ros2 launch eposlib_ros2_examples ros2_control.launch.py dcf:=$CFG/master.dcf interface:=vcan0 > /ws/rc.log 2>&1 & rc=$!
for i in $(seq 1 60); do timeout -s KILL 5 ros2 control list_controllers 2>/dev/null | grep -q "position_controller.*active" && break; sleep 0.5; done
timeout -s KILL 8 ros2 control list_hardware_interfaces 2>/dev/null | sed -n "1,12p"
timeout -s KILL 8 ros2 control list_controllers 2>/dev/null
timeout -s KILL 4 ros2 topic pub -r 10 /position_controller/commands std_msgs/msg/Float64MultiArray "{data: [1.0]}" > /dev/null 2>&1 &
sleep 5
pos=$(timeout -s KILL 8 ros2 topic echo --once /joint_states sensor_msgs/msg/JointState 2>/dev/null | grep -A1 "^position:" | tail -1 | tr -d " -")
echo "shoulder position after a 1.0 rad command: $pos"
if python3 -c "import sys; sys.exit(0 if abs(float(\"$pos\") - 1.0) < 0.05 else 1)" 2>/dev/null; then echo "PASS  ros2_control position"; else echo "FAIL  ros2_control"; grep -iE "error|fatal" /ws/rc.log | head; failures=$((failures+1)); fi
pkill -INT -f "ros2 launch"; sleep 3; pkill -KILL -f "ros2_control_node|spawner|robot_state_publisher|ros2 launch" 2>/dev/null
kill $s; wait $s 2>/dev/null
echo "failures: $failures"
exit $failures
'
