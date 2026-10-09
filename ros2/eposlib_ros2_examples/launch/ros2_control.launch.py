"""Starts the controller manager with the EposSystem hardware interface.

    ros2 launch eposlib_ros2_examples ros2_control.launch.py dcf:=/path/to/master.dcf interface:=can0
"""

from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument
from launch.substitutions import Command, LaunchConfiguration, PathJoinSubstitution
from launch_ros.actions import Node
from launch_ros.parameter_descriptions import ParameterValue
from launch_ros.substitutions import FindPackageShare


def generate_launch_description():
    share = FindPackageShare("eposlib_ros2_examples")
    robot_description = ParameterValue(Command([
        "xacro ", PathJoinSubstitution([share, "urdf", "single_joint.urdf.xacro"]),
        " dcf:=", LaunchConfiguration("dcf"),
        " interface:=", LaunchConfiguration("interface"),
    ]), value_type=str)
    controllers = PathJoinSubstitution([share, "config", "controllers.yaml"])

    return LaunchDescription([
        DeclareLaunchArgument("dcf", description="path to the master DCF"),
        DeclareLaunchArgument("interface", default_value="can0"),
        Node(package="controller_manager", executable="ros2_control_node",
             parameters=[{"robot_description": robot_description}, controllers],
             output="screen"),
        Node(package="robot_state_publisher", executable="robot_state_publisher",
             parameters=[{"robot_description": robot_description}]),
        Node(package="controller_manager", executable="spawner",
             arguments=["joint_state_broadcaster"]),
        Node(package="controller_manager", executable="spawner",
             arguments=["position_controller"]),
    ])
