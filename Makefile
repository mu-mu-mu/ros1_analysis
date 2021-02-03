
lhook: hook_libroscpp.cpp
	g++ -g -Wall -shared -fPIC -ldl hook_libroscpp.cpp -o lhook.so 

