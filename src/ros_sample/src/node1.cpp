#include <chrono>
#include <unistd.h>
#include "ros/ros.h"
#include "ros_sample/nulldata.h"
#include <fstream>
#include <iostream>

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
  //std::chrono::system_clock::time_point  start, end;
  //start = std::chrono::system_clock::now();
  static int count = 0;
  // Calculation from here
  //usleep(10 * 1000); // 10ms
  // Calculation to here
  {
    auto tt = ros::Time::now();
    auto d = ros::Time::now() - tt;
    std::cout <<"po " << d.nsec << std::endl;

    ros_sample::nulldata msg;
    msg.timer = ros::Time::now();

    pub_.publish(msg);




  }

  if(count == 0x1000) {
    ros::shutdown() ;
  }
  count++;

  //end = std::chrono::system_clock::now();

  //std::ofstream outputfile("/home/mumumu/ros1_analysis/test.txt",std::ios::app);
  //outputfile << std::chrono::duration_cast<std::chrono::nanoseconds>(end-start).count() << std::endl;
  //std::cout << std::chrono::duration_cast<std::chrono::nanoseconds>(end-start).count() << std::endl;
  //outputfile.close();
}

int main(int argc, char **argv) {
  ros::init(argc, argv, "node1");


  std::cout << "start" << std::endl;


  Node1 node1;
  ros::spin();
  return 0;
}
