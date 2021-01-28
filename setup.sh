#!/bin/bash
mkdir -p /path/to/project
cd /path/to/project
mkdir src
catkin_make
cd src
git clone https://github.com/sykwer/ros_samples.git
cd ..
catkin_make
source devel/setup.bash
