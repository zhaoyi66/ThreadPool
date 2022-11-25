#ifndef _SEMAPHORE_H
#define _SEMAPHORE_H

#include <mutex>
#include <condition_variable>
#include <atomic>

/*
信号量类
为什么不直接用atomic?
使用atomic一次只允许一个线程访问
只要信号量>0 线程将可以进入 用于线程间通信
*/
class Semaphore
{
public:
    Semaphore(int limit = 0);
    ~Semaphore();

    // 获取一个信号量资源
    void wait();

    //增加一个信号量资源
    void post();

private:
    std::atomic_bool isExit_; // Result对象可能析构了 对应的信号量对象资源析构了 不应该访问不存在的资源
    int resLimit_;
    std::mutex mtx_;
    std::condition_variable cond_;
};

#endif