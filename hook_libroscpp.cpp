#include <stdint.h>
#include <string>
#include <dlfcn.h>
#include <stdio.h>
#include <iostream>
#include <thread>
#include <boost/lockfree/queue.hpp>
#include <chrono>
#include <unistd.h>
#include <sys/types.h>
#include <sys/syscall.h>
#include <fstream>
#include <stdlib.h>


#include "ros/ros.h"
#include "ros/callback_queue.h"
#include "ros/message_deserializer.h"

// Dirty trick!
#define private public
#include "ros/subscription_queue.h"


using namespace std;

enum class fun_type {
  Publication_publish,
  Publication_enqueueMessage,

  SubscriptionQueue_push,
  SubscriptionQueue_call,
};

struct _data {
  fun_type type;
  pid_t pid;
  pid_t tid;
  uint64_t ns;
  uint32_t seq1;
  uint32_t seq2;
};

thread th;
boost::lockfree::queue<_data> queue(4096);

void Logger(const std::string& name) {
  struct _data data = {};
  string homeDir = getenv("HOME");

  cout << "Logger Created" << endl;

  ofstream logfile(homeDir + "/.ros/pl_dir/log_" + name + "_" + to_string(getpid()));
  if (!logfile) {
    cout << "error: couldn't open log file" << endl;
    return;
  }

  for (;;) {
    while (queue.pop(data)) {
      logfile << static_cast<int>(data.type) << " " 
        << data.ns << " " << data.pid << " " 
        << data.tid << " " << data.seq1 << " " << data.seq2 << "\n";
    }
    logfile.flush();

    this_thread::sleep_for(chrono::milliseconds(1000));
  }
}

using init_type = void (*)(int &, char **, const std::string&, uint32_t);

extern "C" void _ZN3ros4initERiPPcRKNSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEEj(int &argc, char **argv, const std::string& name, uint32_t options = 0) {
  static void *orig = NULL;
  if (orig == NULL) {
    orig = dlsym(RTLD_NEXT, "_ZN3ros4initERiPPcRKNSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEEj");
  }

  ((init_type)orig)(argc, argv, name, options);
  cout << "[Detected] start node: " << name << endl;

  th = thread(Logger, name);
}

using enqueueMessage_type = bool (*)(void* ,void*);

extern "C" bool _ZN3ros11Publication14enqueueMessageERKNS_17SerializedMessageE(void *p, void *q) {
  static void *orig = NULL;
  if (orig == NULL) {
    orig = dlsym(RTLD_NEXT, "_ZN3ros11Publication14enqueueMessageERKNS_17SerializedMessageE");
  }

  auto now = chrono::system_clock::now().time_since_epoch();
  auto m = reinterpret_cast<ros::SerializedMessage*>(q);

  uint32_t seq1 = *(uint32_t*)m->message_start;

  bool res = ((enqueueMessage_type)orig)(p,q);

  uint32_t seq2 = *(uint32_t*)m->message_start;

  struct _data data = {};
  data.ns = std::chrono::duration_cast<chrono::nanoseconds>(now).count();
  data.pid = getpid();
  data.tid = syscall(SYS_gettid);
  data.type = fun_type::Publication_enqueueMessage;
  data.seq1 = seq1;
  data.seq2 = seq2;

  queue.push(data);
  
  return res;
}

using publish_type = bool (*)(void* ,void*);

extern "C" bool _ZN3ros11Publication7publishERNS_17SerializedMessageE(void *p, void *q) {
  static void *orig = NULL;
  if (orig == NULL) {
    orig = dlsym(RTLD_NEXT, "_ZN3ros11Publication7publishERNS_17SerializedMessageE");
  }
  static uint32_t seq2 = 0;
  auto now = chrono::system_clock::now().time_since_epoch();
  auto m = reinterpret_cast<ros::SerializedMessage*>(q);

  bool res = ((publish_type)orig)(p,q);

  *(uint32_t*)m->message_start = seq2;

  struct _data data = {};
  data.ns = std::chrono::duration_cast<chrono::nanoseconds>(now).count();
  data.pid = getpid();
  data.tid = syscall(SYS_gettid);
  data.type = fun_type::Publication_publish;
  data.seq1 = 0;
  data.seq2 = seq2++;

  queue.push(data);

  return res;
}

using namespace ros;

extern "C"  CallbackInterface::CallResult _ZN3ros17SubscriptionQueue4callEv(void *p) {
  auto s = reinterpret_cast<SubscriptionQueue*>(p);

  boost::shared_ptr<SubscriptionQueue> self;
  boost::recursive_mutex::scoped_try_lock lock(s->callback_mutex_, boost::defer_lock);

  if (!s->allow_concurrent_callbacks_)
  {
    lock.try_lock();
    if (!lock.owns_lock())
    {
      return CallbackInterface::TryAgain;
    }
  }

  VoidConstPtr tracker;
  SubscriptionQueue::Item i;

  {
    boost::mutex::scoped_lock lock(s->queue_mutex_);

    if (s->queue_.empty())
    {
      return CallbackInterface::Invalid;
    }

    i = s->queue_.front();

    if (s->queue_.empty())
    {
      return CallbackInterface::Invalid;
    }

    if (i.has_tracked_object)
    {
      tracker = i.tracked_object.lock();

      if (!tracker)
      {
        return CallbackInterface::Invalid;
      }
    }

    s->queue_.pop_front();
    --(s->queue_size_);
  }

  VoidConstPtr msg = i.deserializer->deserialize();

  // msg can be null here if deserialization failed
  if (msg)
  {
    try
    {
      //self = shared_from_this();
    }
    catch (boost::bad_weak_ptr&) // For the tests, where we don't create a shared_ptr
    {}

    SubscriptionCallbackHelperCallParams params;
    params.event = MessageEvent<void const>(msg, i.deserializer->getConnectionHeader(), i.receipt_time, i.nonconst_need_copy, MessageEvent<void const>::CreateFunction());

    // Measurement
    {
      struct _data data = {};
      auto now = chrono::system_clock::now().time_since_epoch();
      data.ns = std::chrono::duration_cast<chrono::nanoseconds>(now).count();
      data.pid = getpid();
      data.tid = syscall(SYS_gettid);
      data.type = fun_type::SubscriptionQueue_call;
      data.seq1 = *(uint32_t*)msg.get();
      data.seq2 = 0;

      queue.push(data);
    }
    i.helper->call(params);
  }

  return ros::CallbackInterface::Success;
}

extern "C" void _ZN3ros17SubscriptionQueue4pushERKN5boost10shared_ptrINS_26SubscriptionCallbackHelperEEERKNS2_INS_19MessageDeserializerEEEbRKNS1_8weak_ptrIKvEEbNS_4TimeEPb
                    (void *p, const SubscriptionCallbackHelperPtr& helper,
                     const MessageDeserializerPtr& deserializer,
                     bool has_tracked_object, const VoidConstWPtr& tracked_object,
                     bool nonconst_need_copy, ros::Time receipt_time, bool* was_full)
{
  auto s = reinterpret_cast<SubscriptionQueue*>(p);

  boost::mutex::scoped_lock lock(s->queue_mutex_);

  if (was_full)
  {
    *was_full = false;
  }

  if(s->fullNoLock())
  {
    s->queue_.pop_front();
    --(s->queue_size_);

    if (!s->full_)
    {
      //ROS_DEBUG("Incoming queue was full for topic \"%s\". Discarded oldest message (current queue size [%d])", topic_.c_str(), (int)queue_.size());
    }

    s->full_ = true;

    if (was_full)
    {
      *was_full = true;
    }
  }
  else
  {
    s->full_ = false;
  }

  SubscriptionQueue::Item i;
  i.helper = helper;
  i.deserializer = deserializer;
  i.has_tracked_object = has_tracked_object;
  i.tracked_object = tracked_object;
  i.nonconst_need_copy = nonconst_need_copy;
  i.receipt_time = receipt_time;
  s->queue_.push_back(i);
  ++(s->queue_size_);

  // Measurement
  {
    struct _data data = {};
    auto now = chrono::system_clock::now().time_since_epoch();
    VoidConstPtr msg = deserializer->deserialize();

    data.ns = std::chrono::duration_cast<chrono::nanoseconds>(now).count();
    data.pid = getpid();
    data.tid = syscall(SYS_gettid);
    data.type = fun_type::SubscriptionQueue_push;
    data.seq1 = *(uint32_t*)msg.get();
    data.seq2 = *(uint32_t*)msg.get();

    queue.push(data);
  }
}
