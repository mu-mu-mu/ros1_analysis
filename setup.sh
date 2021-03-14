#!/bin/bash
source /opt/ros/noetic/setup.bash
mkdir -p src
catkin_make
cd src
git clone https://github.com/sykwer/ros_samples.git
cd ..
catkin_make
