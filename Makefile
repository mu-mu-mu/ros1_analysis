SHELL=/bin/bash
C_DIR=$(shell pwd)

lhook.so: hook_libroscpp.cpp
	g++ -g -Wall -shared -fPIC -pthread -ldl -I/opt/ros/noetic/include hook_libroscpp.cpp -o lhook.so 

run:
	source /opt/ros/noetic/setup.sh
	source ./devel/setup.sh
	roslaunch ros_sample0 ros_sample0.launch

run_w_lhook: lhook.so
	source /opt/ros/noetic/setup.sh
	source ./devel/setup.sh
	mkdir -p ~/.ros/pl_dir
	LD_PRELOAD=$(C_DIR)/lhook.so roslaunch ros_sample0 ros_sample0.launch
