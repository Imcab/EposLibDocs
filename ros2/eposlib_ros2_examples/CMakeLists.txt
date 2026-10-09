cmake_minimum_required(VERSION 3.18)
project(eposlib_ros2_examples LANGUAGES CXX)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

find_package(ament_cmake REQUIRED)
find_package(eposlib 0.1 REQUIRED)
find_package(rclcpp REQUIRED)
find_package(rclcpp_lifecycle REQUIRED)
find_package(geometry_msgs REQUIRED)
find_package(sensor_msgs REQUIRED)
find_package(diagnostic_msgs REQUIRED)
find_package(std_srvs REQUIRED)
find_package(hardware_interface REQUIRED)
find_package(pluginlib REQUIRED)

# A node for a differential-drive base.
add_executable(diff_drive_node src/diff_drive_node.cpp)
target_link_libraries(diff_drive_node eposlib::eposlib)
ament_target_dependencies(diff_drive_node rclcpp geometry_msgs sensor_msgs diagnostic_msgs std_srvs)

# A ros2_control hardware interface, loaded as a plugin by the controller manager.
add_library(eposlib_ros2_control SHARED src/epos_system.cpp)
target_include_directories(eposlib_ros2_control PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>)
target_link_libraries(eposlib_ros2_control eposlib::eposlib)
ament_target_dependencies(eposlib_ros2_control rclcpp rclcpp_lifecycle hardware_interface pluginlib)
pluginlib_export_plugin_description_file(hardware_interface epos_system_plugin.xml)

install(TARGETS diff_drive_node DESTINATION lib/${PROJECT_NAME})
install(TARGETS eposlib_ros2_control
  LIBRARY DESTINATION lib
  ARCHIVE DESTINATION lib
  RUNTIME DESTINATION bin)
install(DIRECTORY include/ DESTINATION include)
install(DIRECTORY config launch urdf DESTINATION share/${PROJECT_NAME})

ament_package()
