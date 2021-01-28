# ROS1 ANALYSIS
サンプルとしてsykwerさんのROS1レポジトリに対して実行してみようと思います．
bcc/ros1のインストールは各自で
## Setup
1. git clone
1. ./setup.sh # sykwerさんのレポジトリをクローン(不必要なら自分でよしなに設定してください．)

## Exec
1. sudo python <target>.py
1. roslaunch ros_sample0 ros_sample0.launch # 別のTerminalで

### target
ros_init: とりあえずROS1にuprobe/eBPFが使える確認
moinitor_pub_queue: pub_queueのトレース
