# ROS1 ANALYSIS
サンプルとしてsykwerさんのROS1レポジトリに対して実行してみようと思います．
bcc/ros1のインストールは各自で

## Setup
1. git clone
1. ./setup.sh # sykwerさんのレポジトリをクローン(不必要なら自分でよしなに設定してください．)

## eBPF 系
1. sudo python <target>.py
1. make run # 別のTerminalで，sykwerさんのレポジトリの内容を実行

### target
ros_init: とりあえずROS1にuprobe/eBPFが使える確認
moinitor_pub_queue: pub_queueのトレース

## hook
1. make run_w_lhook

# Data
python3 calc_e2e.py intra autoware_data_01/log_remote_cmd_converter_10041
python3 calc_e2e.py inter autoware_data_01/log_multi_object_tracker_9853 autoware_data_01/log_map_based_prediction_9873

# Lisence
This software is released under the Apache License, Version 2.0. See LICENSE. 
qlat_text.py: This file includes the modified code of iovisor/bcc
which is released under Apache License 2.0. 
hook_libroscpp.cpp: This file includes the modified code of ROS(ros_comm)
which is released under BSD License.
