#!/bin/bash

# 编译生成的.so和相关的.h放到PATH路径下
g++ -fPIC -shared Result.cpp Semaphore.cpp Task.cpp Thread.cpp ThreadPool.cpp  -o libpool.so -std=c++17

cp libpool.so /usr/local/lib/
cp *.h /usr/local/include/

# 让动态库生效
echo "/usr/local/lib" > /etc/ld.so.conf.d/mylib.conf
ldconfig

