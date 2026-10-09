cmake_minimum_required(VERSION 3.18)
project(eposlib_examples LANGUAGES CXX)

# Every complete program in the EposLib documentation. Plain CMake, like
# EposLib itself: builds with colcon in a ROS workspace, or with cmake alone
# once eposlib is on CMAKE_PREFIX_PATH.
find_package(eposlib 0.1 REQUIRED)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

set(EXAMPLES
  device_info
  profile_position
  profile_velocity
  cyclic_velocity
  cyclic_torque
  cyclic_position
  homing
  multi_drive
  fault_monitor
  configuration
  encoder_setup
  digital_io
  step_response
  telemetry_logger
)

foreach(example ${EXAMPLES})
  add_executable(${example} src/${example}.cpp)
  target_link_libraries(${example} PRIVATE eposlib::eposlib)
  install(TARGETS ${example} DESTINATION lib/${PROJECT_NAME})
endforeach()

# The two-drive network used by multi_drive, turned into a master DCF.
eposlib_generate_dcf(two_drives DESTINATION share/${PROJECT_NAME}/config)

install(FILES package.xml DESTINATION share/${PROJECT_NAME})
file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/ament_index_marker "")
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/ament_index_marker
  DESTINATION share/ament_index/resource_index/packages
  RENAME ${PROJECT_NAME})
