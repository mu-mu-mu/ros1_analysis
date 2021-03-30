#include <chrono>
#include <unistd.h>
#include "ros/ros.h"
#include "ros_sample/nulldata.h"

using namespace std::chrono;

class Node2 {
  ros::NodeHandle nh_;
  ros::Subscriber sub_;
  ros::Publisher pub_;

  void callback(const ros_sample::nulldata::ConstPtr &input_msg);
public:
  Node2();
};

Node2::Node2() : nh_("~") {
  sub_ = nh_.subscribe("input", 10, &Node2::callback, this);
  pub_ = nh_.advertise<ros_sample::nulldata>("output", 10);
}


void Node2::callback(const ros_sample::nulldata::ConstPtr &input_msg) {

  static int count = 0;
  if(count == 0x1000) {
    ros::shutdown() ;
  }
  count++;

}

int main(int argc, char **argv) {
  ros::init(argc, argv, "node2");
  Node2 node2;
  ros::spin();

  return 0;
}
