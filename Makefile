SHELL=/bin/bash
C_DIR=$(shell pwd)

lhook.so: hook_libroscpp.cpp demangler
	g++ -g -Wall -shared -fPIC -pthread -ldl -L /usr/local/lib -I/opt/ros/melodic/include -I$(C_DIR)/devel/include hook_libroscpp.cpp -o lhook.so

demangler: demangler.cpp
	g++ -o demangler demangler.cpp

run:
	source /opt/ros/melodic/setup.sh
	source ./devel/setup.sh
	roslaunch ros_sample0 ros_sample0.launch

run0_w_lhook: lhook.so
	source /opt/ros/melodic/setup.sh
	source ./devel/setup.sh
	mkdir -p ~/.ros/pl_dir
	LD_PRELOAD="/usr/lib/x86_64-linux-gnu/libboost_system.so $(C_DIR)/lhook.so" roslaunch ros_sample0 ros_sample0.launch

run1_w_lhook: lhook.so
	source /opt/ros/melodic/setup.sh
	source ./devel/setup.sh
	mkdir -p ~/.ros/pl_dir
	LD_PRELOAD="/usr/lib/x86_64-linux-gnu/libboost_system.so $(C_DIR)/lhook.so" roslaunch ros_sample1 ros_sample1.launch

run_w_lhook: lhook.so
	source /opt/ros/melodic/setup.sh
	source ./devel/setup.sh
	mkdir -p ~/.ros/pl_dir
	LD_PRELOAD="/usr/lib/x86_64-linux-gnu/libboost_system.so $(C_DIR)/lhook.so" roslaunch ros_sample ros_sample.launch

clean_pl_data:
	rm ~/.ros/pl_dir/log*
