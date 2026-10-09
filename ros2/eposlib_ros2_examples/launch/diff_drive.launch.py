"""Starts the differential-drive node.

    ros2 launch eposlib_ros2_examples diff_drive.launch.py dcf:=/path/to/master.dcf
"""

from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument
from launch.substitutions import LaunchConfiguration, PathJoinSubstitution
from launch_ros.actions import Node
from launch_ros.substitutions import FindPackageShare


def generate_launch_description():
    params = PathJoinSubstitution([FindPackageShare("eposlib_ros2_examples"), "config", "diff_drive.yaml"])
    return LaunchDescription([
        DeclareLaunchArgument("dcf", description="path to the master DCF"),
        DeclareLaunchArgument("interface", default_value="can0"),
        Node(package="eposlib_ros2_examples", executable="diff_drive_node", name="diff_drive",
             parameters=[params, {"dcf": LaunchConfiguration("dcf"),
                                  "interface": LaunchConfiguration("interface")}],
             output="screen"),
    ])
