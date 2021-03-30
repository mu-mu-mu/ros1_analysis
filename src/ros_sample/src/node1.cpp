#include <chrono>
#include <unistd.h>
#include "ros/ros.h"
#include "ros_sample/nulldata.h"

using namespace std::chrono;

class Node1 {
  ros::NodeHandle n_;
  ros::Publisher pub_;
  ros::Timer timer_;
  void timerCallback(const ros::TimerEvent &);
public:
  Node1();
};

Node1::Node1() : n_("~") {
  pub_ = n_.advertise<ros_sample::nulldata>("output", 10);
  timer_ = n_.createTimer(ros::Duration(0.1), &Node1::timerCallback, this);
}

void Node1::timerCallback(const ros::TimerEvent &) {
  static int count = 0;
  // Calculation from here
  //usleep(10 * 1000); // 10ms
  // Calculation to here
  {
    ros_sample::nulldata msg;

    pub_.publish(msg);

  }

  if(count == 0x1000) {
    ros::shutdown() ;
  }
  count++;
}

int main(int argc, char **argv) {
  ros::init(argc, argv, "node1");
  Node1 node1;
  ros::spin();
  return 0;
}
