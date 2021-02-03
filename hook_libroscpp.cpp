#include<stdint.h>
#include<string>
#include<dlfcn.h>
#include<stdio.h>
#include<iostream>

using namespace std;

using init_type = void (*)(int &, char **, const std::string&, uint32_t);

namespace ros {

void init(int &argc, char **argv, const std::string& name, uint32_t options = 0) {
  void *orig_init = dlsym(RTLD_NEXT, "_ZN3ros4initERiPPcRKNSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEEj");

  ((init_type)orig_init)(argc, argv, name, options);
  cout << "[Detected] start node: " << name << endl;
}

}
