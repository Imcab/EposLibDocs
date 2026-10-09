#!/usr/bin/env bash
# Builds every example against EposLib and runs those epos4_sim can host,
# on a vcan0 inside a throwaway container. Nothing touches the host network.
#
#   scripts/check_examples.sh [path/to/EposLib] [path/to/ros2units]
#
# Needs Docker and an image with ROS 2 Humble and Lely
# (ros-humble-lely-core-libraries); set IMAGE to use another one.
set -euo pipefail

DOCS="$(cd "$(dirname "$0")/.." && pwd)"
EPOSLIB="$(cd "${1:-$HOME/ros2_ws/src/EposLib}" && pwd)"
UNITS="$(cd "${2:-$HOME/ros2_ws/src/ros2units}" && pwd)"
IMAGE="${IMAGE:-ros2_humble_gazebo}"

docker run --rm --privileged --user root -e SHOW="${SHOW:-4}" \
  -v "$EPOSLIB":/src/EposLib:ro \
  -v "$UNITS":/src/ros2units:ro \
  -v "$DOCS/examples":/src/eposlib_examples:ro \
  "$IMAGE" bash -c '
set -o pipefail
ip link add dev vcan0 type vcan && ip link set up vcan0
mkdir -p /ws/src && cp -r /src/EposLib /src/ros2units /src/eposlib_examples /ws/src/
cd /ws && source /opt/ros/humble/setup.bash
if ! colcon build --cmake-args -DBUILD_TESTING=OFF > build.log 2>&1; then
  tail -40 build.log; exit 1
fi
if grep -E "warning:" build.log; then echo "BUILD WARNINGS"; exit 1; fi
echo "build: all examples compiled without warnings"
source install/setup.bash

SIM=/ws/install/eposlib/lib/eposlib/epos4_sim
BIN=/ws/install/eposlib_examples/lib/eposlib_examples
CFG=/ws/install/eposlib/share/eposlib/config/epos4_network
TWO=/ws/install/eposlib_examples/share/eposlib_examples/config/two_drives
# The simulator takes «Motor rated torque» from the EDS default, which is 0;
# the torque example needs a motor with data, so give it one.
awk "/^\\[6076\\]/{f=1} f&&/^DefaultValue=/{print \"DefaultValue=450000\"; f=0; next} {print}" \
  $CFG/epos4.eds > /ws/rated.eds

failures=0
run() {  # name, eds, timeout, command...
  local name=$1 eds=$2 limit=$3; shift 3
  $SIM $eds 2 vcan0 > /ws/sim_$name.log 2>&1 & local sim=$!
  sleep 0.5
  timeout -s KILL $limit "$@" > /ws/$name.log 2>&1; local code=$?
  kill $sim 2>/dev/null; wait $sim 2>/dev/null
  if [ $code -eq 0 ]; then echo "PASS  $name"; else echo "FAIL  $name (exit $code)"; failures=$((failures+1)); fi
  sed "s/^/      /" /ws/$name.log | tail -${SHOW:-4}
}

run device_info      $CFG/epos4.eds 30  $BIN/device_info      $CFG/master.dcf vcan0 2
run configuration    $CFG/epos4.eds 30  $BIN/configuration    $CFG/master.dcf vcan0 2 --save
run encoder_setup    $CFG/epos4.eds 30  $BIN/encoder_setup    $CFG/master.dcf vcan0 2
run profile_position $CFG/epos4.eds 120 $BIN/profile_position $CFG/master.dcf vcan0 2
run cyclic_velocity  $CFG/epos4.eds 30  $BIN/cyclic_velocity  $CFG/master.dcf vcan0 2 1000
run cyclic_torque    /ws/rated.eds  30  $BIN/cyclic_torque    $CFG/master.dcf vcan0 2 0.05 5000
run cyclic_position  $CFG/epos4.eds 30  $BIN/cyclic_position  $CFG/master.dcf vcan0 2 20
run homing           $CFG/epos4.eds 60  $BIN/homing           $CFG/master.dcf vcan0 2
run fault_monitor    $CFG/epos4.eds 30  $BIN/fault_monitor    $CFG/master.dcf vcan0 2 3
run digital_io       $CFG/epos4.eds 30  $BIN/digital_io       $CFG/master.dcf vcan0 2

# Two simulated drives, nodes 2 and 3, on the two-drive network.
$SIM $CFG/epos4.eds 3 vcan0 > /ws/sim_node3.log 2>&1 & node3=$!
run multi_drive      $CFG/epos4.eds 30  $BIN/multi_drive      $TWO/master.dcf vcan0
kill $node3 2>/dev/null; wait $node3 2>/dev/null

echo "not run (epos4_sim does not model Profile Velocity): profile_velocity - compiled only"
echo "failures: $failures"
exit $failures
'
