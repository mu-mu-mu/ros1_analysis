cmake_minimum_required(VERSION 3.0.2)
project(ros_sample)

find_package(catkin REQUIRED COMPONENTS
  roscpp
  std_msgs
  message_generation
)

add_message_files(
  FILES
  nulldata.msg
  n256b.msg
  n1kb.msg
  n4kb.msg
  n32kb.msg
  n256kb.msg
  n1mb.msg
  n2mb.msg
  n4mb.msg
)

generate_messages(
  DEPENDENCIES
  std_msgs
)

catkin_package(CATKIN_DEPENDS message_runtime)

include_directories(
  include
  ${catkin_INCLUDE_DIRS}
)

add_executable(${PROJECT_NAME}_node1 src/node1.cpp)
target_link_libraries(${PROJECT_NAME}_node1 ${catkin_LIBRARIES})
add_dependencies(${PROJECT_NAME}_node1 ros_sample_generate_messages_cpp)

add_executable(${PROJECT_NAME}_node2 src/node2.cpp)
target_link_libraries(${PROJECT_NAME}_node2 ${catkin_LIBRARIES})
add_dependencies(${PROJECT_NAME}_node2 ros_sample_generate_messages_cpp)

