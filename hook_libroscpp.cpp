#include<stdint.h>
#include<string>
#include<dlfcn.h>
#include<stdio.h>
#include<iostream>

using namespace std;

namespace ros {
using init_type = void (*)(int &, char **, const std::string&, uint32_t);

void init(int &argc, char **argv, const std::string& name, uint32_t options = 0) {
  void *orig_init = dlsym(RTLD_NEXT, "_ZN3ros4initERiPPcRKNSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEEj");

  ((init_type)orig_init)(argc, argv, name, options);
  cout << "[Detected] start node: " << name << endl;
}
}

using enqueueMessage_type = bool (*)(void* ,void*);

extern "C" bool  _ZN3ros11Publication14enqueueMessageERKNS_17SerializedMessageE(void *p, void *q) {
  void *orig_eM = dlsym(RTLD_NEXT, "_ZN3ros11Publication14enqueueMessageERKNS_17SerializedMessageE");
  cout << "call detect: enqueueMessage 1st arg: " << p << " 2nd arg: " << q << endl;
  return ((enqueueMessage_type)orig_eM)(p,q);
}

using publish_type = bool (*)(void* ,void*);

extern "C" bool _ZN3ros11Publication7publishERNS_17SerializedMessageE(void *p, void *q) {
  void *orig_pub = dlsym(RTLD_NEXT, "_ZN3ros11Publication7publishERNS_17SerializedMessageE");
  cout << "call detect: publish 1st arg: " << p << " 2nd arg: " << q << endl;
  return ((publish_type)orig_pub)(p,q);
}
