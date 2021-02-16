#include<stdint.h>
#include<string>
#include<dlfcn.h>
#include<stdio.h>
#include<iostream>
#include<thread>
#include<boost/lockfree/queue.hpp>
#include<chrono>
#include<unistd.h>
#include<sys/types.h>
#include<sys/syscall.h>
#include<fstream>
#include <stdlib.h>

#include "ros/ros.h"


using namespace std;

enum class fun_type {
  PublicationPublish,
  PublicationenqueueMessage,
};


struct _data {
  fun_type type;
  pid_t pid;
  pid_t tid;
  void* msg;
  uint64_t ns;
};


thread th;
boost::lockfree::queue<_data> queue(4096);

void Logger() {
  struct _data data = {};
  string homeDir = getenv("HOME");

  cout << "Logger Created" << endl;

  ofstream logfile(homeDir + "/.ros/pl_dir/log_" + to_string(getpid()));
  if (!logfile) {
    cout << "error: couldn't open log file" << endl;
    return;
  }

  for (;;) {
    //cout << "Logger" << endl;
    while (queue.pop(data)) {
      logfile << static_cast<int>(data.type) << " " 
        << data.ns << " " << data.pid << " " 
        << data.tid << " " << data.msg << "\n";
    }
    logfile.flush();

    this_thread::sleep_for(chrono::milliseconds(1000));
  }
}

using init_type = void (*)(int &, char **, const std::string&, uint32_t);

extern "C" void _ZN3ros4initERiPPcRKNSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEEj(int &argc, char **argv, const std::string& name, uint32_t options = 0) {
  void *orig_init = dlsym(RTLD_NEXT, "_ZN3ros4initERiPPcRKNSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEEj");

  ((init_type)orig_init)(argc, argv, name, options);
  cout << "[Detected] start node: " << name << endl;

  th = thread(Logger);
}


using enqueueMessage_type = bool (*)(void* ,void*);

extern "C" bool  _ZN3ros11Publication14enqueueMessageERKNS_17SerializedMessageE(void *p, void *q) {
  void *orig_eM = dlsym(RTLD_NEXT, "_ZN3ros11Publication14enqueueMessageERKNS_17SerializedMessageE");
  auto now = chrono::system_clock::now().time_since_epoch();
  struct _data data = {};
  auto m = reinterpret_cast<ros::SerializedMessage*>(p);

  data.ns = std::chrono::duration_cast<chrono::nanoseconds>(now).count();
  data.msg = (void*)m->buf.get();
  data.pid = getpid();
  data.tid = syscall(SYS_gettid);
  data.type = fun_type::PublicationenqueueMessage;

  queue.push(data);
  
  cout << "call detect: Publication::enqueueMessage() " << (void*)&(m->buf) << endl;
  return ((enqueueMessage_type)orig_eM)(p,q);
}

using publish_type = bool (*)(void* ,void*);

extern "C" bool _ZN3ros11Publication7publishERNS_17SerializedMessageE(void *p, void *q) {
  void *orig_pub = dlsym(RTLD_NEXT, "_ZN3ros11Publication7publishERNS_17SerializedMessageE");
  auto now = chrono::system_clock::now().time_since_epoch();
  struct _data data = {};
  auto m = reinterpret_cast<ros::SerializedMessage*>(p);

  data.ns = std::chrono::duration_cast<chrono::nanoseconds>(now).count();
  data.msg = (void*)m->buf.get();
  data.pid = getpid();
  data.tid = syscall(SYS_gettid);
  data.type = fun_type::PublicationPublish;

  queue.push(data);

  cout << "call detect: Publication::publish() " << (void*)&(m->buf) << endl;
  return ((publish_type)orig_pub)(p,q);
}
