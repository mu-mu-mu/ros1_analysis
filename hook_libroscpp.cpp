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
#include <bits/stdc++.h>
#include <sstream>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include "ros/forwards.h"
#include "ros/common.h"
#include "ros/message.h"
#include "ros/serialization.h"
#include "ros/io.h"
#include "ros/poll_set.h"
#include "ros/header.h"

// Dirty trick!
#define private public
#include "ros/transport/transport_tcp.h"
#undef private

#define private public
#include "ros/topic_manager.h"
#undef private

#define private public
#include "ros/publisher.h"
#undef private

#define private public
#include "ros/message_deserializer.h"
#undef private

#include "ros/ros.h"
#include "ros/callback_queue.h"
#include "ros/publication.h"

// Dirty trick!
#define private public
#include "ros/subscription_queue.h"
#undef private


using namespace std;
using namespace ros;

enum class fun_type {
  Publication_publish, //0
  Publisher_publish,
  Publication_enqueueMessage,

  SubscriptionQueue_push, //3
  SubscriptionQueue_call_before_callback,
  SubscriptionQueue_call_after_callback,

  TransportTCP_write, //6
  TransportTCP_read,

  accept,
  connect,
  bind,
};

boost::mutex subscription_queue_push_mutex;

uint32_t push_seq_table[0x1000];

struct _data {
  fun_type type;
  pid_t pid;
  pid_t tid;
  uint64_t ns;
  uint64_t seq1;
  uint64_t seq2;
  const char* name = "none";
};

thread th;
boost::lockfree::queue<_data> data_queue(0x1000);

bool flag_publicaton_enqueueMessage = false;

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
    while (data_queue.pop(data)) {
      logfile << static_cast<int>(data.type) << " " 
        << data.ns << " " << data.pid << " " 
        << data.tid << " " << data.seq1 << " " 
        << data.seq2 << " " << data.name << "\n";
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
// ros::Publication::enqueueMessage(ros::SerializedMessage const&)
extern "C" bool _ZN3ros11Publication14enqueueMessageERKNS_17SerializedMessageE(void *p, void *q) {
  static void *orig = NULL;
  if (orig == NULL) {
    orig = dlsym(RTLD_NEXT, "_ZN3ros11Publication14enqueueMessageERKNS_17SerializedMessageE");
  }

  static pid_t pid = 0, tid = 0;
  if (pid == 0) {
    pid = getpid();
    tid = syscall(SYS_gettid);
    flag_publicaton_enqueueMessage = true;
  }

  auto now = chrono::system_clock::now().time_since_epoch();
  auto m = reinterpret_cast<ros::SerializedMessage*>(q);

  uint32_t seq1 = *(uint32_t*)m->message_start;
  //cout << "enq " << (uint32_t*)m->buf.get() << " " << pid << endl;

  bool res = ((enqueueMessage_type)orig)(p,q);

  uint32_t seq2 = *(uint32_t*)m->message_start;

  struct _data data = {};
  data.ns = std::chrono::duration_cast<chrono::nanoseconds>(now).count();
  data.pid = pid;
  data.tid = tid;
  data.type = fun_type::Publication_enqueueMessage;
  data.seq1 = seq1;
  data.seq2 = seq2;

  data_queue.push(data);
  
  return res;
}

using publish_type = bool (*)(void* ,void*);

// ros::Publication::publish(ros::SerializedMessage&)
extern "C" bool _ZN3ros11Publication7publishERNS_17SerializedMessageE(void *p, void *q) {
  static void *orig = NULL;
  if (orig == NULL) {
    orig = dlsym(RTLD_NEXT, "_ZN3ros11Publication7publishERNS_17SerializedMessageE");
  }
  //static uint32_t seq = 0;

  static pid_t pid = 0, tid = 0;
  if (pid == 0) {
    pid = getpid();
    tid = syscall(SYS_gettid);
  }

  auto m = reinterpret_cast<ros::SerializedMessage*>(q);

  struct _data data = {};
  data.type = fun_type::Publication_publish;
  cout << "push " << (uint32_t*)m->buf.get() << " " << data.pid << endl;
  uint64_t seq1 = (uint64_t)m;
  uint64_t seq2 = (uint64_t)m->buf.get();

  bool res = ((publish_type)orig)(p,q);

  auto now = chrono::system_clock::now().time_since_epoch();
  data.ns = std::chrono::duration_cast<chrono::nanoseconds>(now).count();
  data.pid = pid;
  data.tid = tid;
  data.seq1 = seq1; //*(uint32_t*)m->message_start;
  data.seq2 = seq2; //*(uint32_t*)m->message_start;
  data_queue.push(data);

  return res;
}

extern "C" TopicManagerPtr& _ZN3ros12TopicManager8instanceEv();
//ros::Publisher::publish(boost::function<ros::SerializedMessage ()> const&, ros::SerializedMessage&)
extern "C" void _ZNK3ros9Publisher7publishERKN5boost8functionIFNS_17SerializedMessageEvEEERS3_(void *p, const boost::function<SerializedMessage(void)>& serfunc, SerializedMessage& m)
{
  auto s = reinterpret_cast<Publisher*>(p);

  static struct _data data = {};
  static bool initialized = false;
  if (!initialized) {
    data.pid = getpid();
    data.tid = syscall(SYS_gettid);
    data.type = fun_type::Publisher_publish;
    initialized = true;
  }

  // Inserted
  {
    auto now = chrono::system_clock::now().time_since_epoch();
    data.ns = std::chrono::duration_cast<chrono::nanoseconds>(now).count();
  }

  if (!s->impl_)
  {
    //ROS_ASSERT_MSG(false, "Call to publish() on an invalid Publisher (topic [%s])", impl_->topic_.c_str());
    return;
  }

  if (!s->impl_->isValid())
  {
    //ROS_ASSERT_MSG(false, "Call to publish() on an invalid Publisher (topic [%s])", impl_->topic_.c_str());
    return;
  }

  // Inserted
  PublicationPtr pp = _ZN3ros12TopicManager8instanceEv()->lookupPublicationWithoutLock(s->impl_->topic_);
  if (pp->hasSubscribers() || pp->isLatching())
  {
    cout << "push0 " << &m << " " << data.tid << endl;
    
    _ZN3ros12TopicManager8instanceEv()->publish(s->impl_->topic_, serfunc, m);

    data.seq1 = (uint64_t)&m; //*(uint32_t*)m.message_start;
    data.seq2 = (uint64_t)&m; //*(uint32_t*)m.message_start;
    data_queue.push(data);
  }
  else {
    _ZN3ros12TopicManager8instanceEv()->publish(s->impl_->topic_, serfunc, m);
  }

  // For Melodic
  /*
  if (s->isLatched()) {
    boost::mutex::scoped_lock lock(s->impl_->last_message_mutex_);
    s->impl_->last_message_ = m;
  }
  */
}

extern "C"  CallbackInterface::CallResult _ZN3ros17SubscriptionQueue4callEv(void *p) {
  auto s = reinterpret_cast<SubscriptionQueue*>(p);
  auto now = chrono::system_clock::now().time_since_epoch();
  static pid_t pid = 0, tid = 0;
  if (pid == 0) {
    pid = getpid();
    tid = syscall(SYS_gettid);
  }

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
      data.pid = pid;
      data.tid = tid;
      data.type = fun_type::SubscriptionQueue_call_before_callback;

      data.seq1 = *(uint32_t*)msg.get();
      //*(uint32_t*)msg.get() = push_seq_table[data.seq1 % 0x1000];

      data.seq2 = push_seq_table[data.seq1 % 0x1000];
      data.name = i.helper->getTypeInfo().name();

      data_queue.push(data);

    }
    i.helper->call(params);
    // Measurement
    {
      struct _data data = {};
      auto now = chrono::system_clock::now().time_since_epoch();
      data.ns = std::chrono::duration_cast<chrono::nanoseconds>(now).count();
      data.pid = pid;
      data.tid = tid;
      data.type = fun_type::SubscriptionQueue_call_after_callback;
      data.seq1 = *(uint32_t*)msg.get();
      data.seq2 = 0;

      data_queue.push(data);
    }
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

  static uint32_t seq_push = 0;
  static struct _data data = {};
  static bool initialized = false;
  if (!initialized) {
    data.type = fun_type::SubscriptionQueue_push;
    data.pid = getpid();
    data.tid = syscall(SYS_gettid);
  }

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
    auto now = chrono::system_clock::now().time_since_epoch();
    ros::SerializedMessage* m = &(deserializer->serialized_message_);

    cout << "push1 " << m << " " << data.pid << endl;
    //VoidConstPtr msg = deserializer->deserialize();

    {
      boost::mutex::scoped_lock push_lock(subscription_queue_push_mutex);
      data.seq2 = seq_push;
      seq_push++;
    }

    data.ns = std::chrono::duration_cast<chrono::nanoseconds>(now).count();
    //data.seq1 = *(uint32_t*)m->message_start;
    //data.seq1 = *(uint32_t*)msg.get();
    //
    
    //*(uint32_t*)m->message_start = data.seq2;
    //*(uint32_t*)msg.get() = data.seq2;
    data_queue.push(data);

    push_seq_table[data.seq2 % 0x1000] = data.seq1;
  }
}
/*
using send_type = ssize_t (*)(int, const void *, size_t, int);

extern "C" ssize_t send(int sockfd, const void *buf, size_t len, int flags) {
  static void *orig = NULL;
  if (orig == NULL) {
    orig = dlsym(RTLD_NEXT, "send");
  }

  static pid_t pid = 0, tid = 0;
  if (pid == 0) {
    pid = getpid();
    tid = syscall(SYS_gettid);
  }

  //cout << *(uint32_t*)buf << " "  << tid << endl;
  return ((send_type)orig)(sockfd, buf, len, flags);
}*/

/*
using write_type = ssize_t (*)(int, const void *, size_t);

extern "C" ssize_t write(int fd, const void *buf, size_t count) {
  static void *orig = NULL;
  if (orig == NULL) {
    orig = dlsym(RTLD_NEXT, "write");
  }
  return ((write_type)orig)(fd, buf, count);
}*/

// ros::TransportTCP::write(unsigned char*, unsigned int)
extern "C" int32_t _ZN3ros12TransportTCP5writeEPhj(void* p, uint8_t* buffer, uint32_t size) {
  auto s = reinterpret_cast<TransportTCP*>(p);

  static struct _data data = {};
  static bool initialized = false;
  if (!initialized) {
    data.pid = getpid();
    data.tid = syscall(SYS_gettid);
    data.type = fun_type::TransportTCP_write;
    initialized = true;
  }


  {
    boost::recursive_mutex::scoped_lock lock(s->close_mutex_);

    // For Melodic
    /*
    // if socket is async and not connected, check if it's conneted
    if (!(s->flags_ & s->SYNCHRONOUS) && !s->async_connected_ && !s->closed_) {
      int ret, err;
      ret = is_async_connected(s->sock_, err);
      if (ret == 1) {
        //ROSCPP_CONN_LOG_DEBUG("Async socket[%d] is connected", sock_);
        s->async_connected_ = true;
      } else if (ret == -1) {
        //ROSCPP_LOG_DEBUG("Async connect on socket [%d] failed with error [%s]", sock_, socket_error_string(err));
        s->close();
      } else {
        // socket is connecting
        return 0;
      }
    }
    */

    if (s->closed_)
    {
      //ROSCPP_LOG_DEBUG("Tried to write on a closed socket [%d]", sock_);
      return -1;
    }
  }

  //ROS_ASSERT(size > 0);

  // never write more than INT_MAX since this is the maximum we can report back with the current return type
  uint32_t writesize = std::min(size, static_cast<uint32_t>(INT_MAX));

  // Inserted: Measurement
  if (flag_publicaton_enqueueMessage) {
    //cout << ((uint32_t*)buffer)[1] << " "  << endl;
    auto now = chrono::system_clock::now().time_since_epoch();
    data.ns = std::chrono::duration_cast<chrono::nanoseconds>(now).count();
    data.seq1 = ((uint32_t*)buffer)[1];
    data.seq2 = ((uint32_t*)buffer)[1];

    data_queue.push(data);
  }

  int num_bytes = ::send(s->sock_, reinterpret_cast<const char*>(buffer), writesize, 0);
  if (num_bytes < 0)
  {
    if ( !last_socket_error_is_would_block() )
    {
      //ROSCPP_LOG_DEBUG("send() on socket [%d] failed with error [%s]", sock_, last_socket_error_string());
      s->close();
    }
    else
    {
      num_bytes = 0;
    }
  }

  return num_bytes;
}

// ros::TransportTCP::read(unsigned char*, unsigned int)
extern "C" int32_t _ZN3ros12TransportTCP4readEPhj(void *p, uint8_t* buffer, uint32_t size)
{
  auto s = reinterpret_cast<TransportTCP*>(p);

  static struct _data data = {};
  static bool initialized = false;
  if (!initialized) {
    data.pid = getpid();
    data.tid = syscall(SYS_gettid);
    data.type = fun_type::TransportTCP_read;
    initialized = true;
  }


  {
    boost::recursive_mutex::scoped_lock lock(s->close_mutex_);

    // For Melodic
    /*
    // if socket is async and not connected, check if it's conneted
    if (!(s->flags_ & s->SYNCHRONOUS) && !s->async_connected_ && !s->closed_) {
      int ret, err;
      ret = is_async_connected(s->sock_, err);
      if (ret == 1) {
        //ROSCPP_CONN_LOG_DEBUG("Async socket[%d] is connected", sock_);
        s->async_connected_ = true;
      } else if (ret == -1) {
        //ROSCPP_LOG_DEBUG("Async connect on socket [%d] failed with error [%s]", sock_, socket_error_string(err));
        s->close();
      } else {
        // socket is connecting
        return 0;
      }
    }
    */

    if (s->closed_)
    {
      //ROSCPP_LOG_DEBUG("Tried to read on a closed socket [%d]", sock_);
      return -1;
    }
  }

  //ROS_ASSERT(size > 0);

  // never read more than INT_MAX since this is the maximum we can report back with the current return type
  uint32_t read_size = std::min(size, static_cast<uint32_t>(INT_MAX));
  int num_bytes = ::recv(s->sock_, reinterpret_cast<char*>(buffer), read_size, 0);

  // Inserted: Measurement
  if (num_bytes > 4)
  {
    //cout <<  s->sock_ <<" " <<((uint32_t*)buffer)[0] << " " << data.pid  << endl;
    auto now = chrono::system_clock::now().time_since_epoch();
    data.ns = std::chrono::duration_cast<chrono::nanoseconds>(now).count();
    data.seq1 = s->sock_;
    data.seq2 = ((uint32_t*)buffer)[0];

    data_queue.push(data);
  }

  if (num_bytes < 0)
  {
	if ( !last_socket_error_is_would_block() ) // !WSAWOULDBLOCK / !EAGAIN && !EWOULDBLOCK
    {
      //ROSCPP_LOG_DEBUG("recv() on socket [%d] failed with error [%s]", sock_, last_socket_error_string());
      s->close();
    }
    else
    {
      num_bytes = 0;
    }
  }
  else if (num_bytes == 0)
  {
    //ROSCPP_LOG_DEBUG("Socket [%d] received 0/%u bytes, closing", sock_, size);
    s->close();
    return -1;
  }

  return num_bytes;
}

using accept_type = int (*)(int sockfd, struct sockaddr *addr, socklen_t *addrlen);
extern "C" int accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen) {
  static void *orig = NULL;
  if (orig == NULL) {
    orig = dlsym(RTLD_NEXT, "accept");
  }

  static struct _data data = {};
  static bool initialized = false;
  if (!initialized) {
    data.pid = getpid();
    data.tid = syscall(SYS_gettid);
    data.type = fun_type::accept;
    initialized = true;
  }

  int res = ((accept_type)orig)(sockfd, addr, addrlen);

  {
    // cout << "accept :" << sockfd << " " <<  ntohs(((sockaddr_in*)addr)->sin_port) << " " << data.pid <<endl;
    data.seq1 = ntohs(((sockaddr_in*)addr)->sin_port);
    data.seq2 = sockfd;
    data_queue.push(data);
  }
  return res;
}

using connect_type = int (*)(int sockfd, const struct sockaddr *addr, socklen_t addrlen);
extern "C" int connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen) {
  static void *orig = NULL;
  if (orig == NULL) {
    orig = dlsym(RTLD_NEXT, "connect");
  }

  static struct _data data = {};
  static bool initialized = false;
  if (!initialized) {
    data.pid = getpid();
    data.tid = syscall(SYS_gettid);
    data.type = fun_type::connect;
    initialized = true;
  }

  int res = ((connect_type)orig)(sockfd, addr, addrlen);
  {
    // cout << "connect :" << sockfd << " " << ntohs(((sockaddr_in*)addr)->sin_port) << " " << data.pid <<endl;
    data.seq1 = ntohs(((sockaddr_in*)addr)->sin_port);
    data.seq2 = sockfd;
    data_queue.push(data);
  }
  return res;
}

using bind_type = int (*)(int sockfd, const struct sockaddr *addr, socklen_t addrlen);
extern "C" int bind(int sockfd, const struct sockaddr *addr, socklen_t addrlen) {
  static void *orig = NULL;
  if (orig == NULL) {
    orig = dlsym(RTLD_NEXT, "bind");
  }

  static struct _data data = {};
  static bool initialized = false;
  if (!initialized) {
    data.pid = getpid();
    data.tid = syscall(SYS_gettid);
    data.type = fun_type::bind;
    initialized = true;
  }

  int res = ((bind_type)orig)(sockfd, addr, addrlen);

  struct sockaddr_in addr_;
  bzero(&addr_,sizeof(addr_));
  socklen_t addrlen_ = sizeof(addr_);
  getsockname(sockfd, (sockaddr*)&addr_, &addrlen_);

  {
    // cout << "bind :" << sockfd << " " << ntohs(addr_.sin_port) << " " << data.pid <<endl;
    data.seq1 = ntohs(addr_.sin_port);
    data.seq2 = sockfd;
    data_queue.push(data);
  }

  return res;
}
