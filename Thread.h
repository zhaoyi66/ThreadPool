#ifndef _THREAD_H
#define _THREAD_H

#include <functional>
/*
线程包装类 保存要执行的函数指针
在适当的时候定义函数对象开启线程
*/
class Thread
{
public:
    //封装一类函数指针
    using ThreadFunc = std::function<void(int)>;//指向ThreadPool::threadFunc任务分配函数

    Thread(ThreadFunc func);
    ~Thread();

    // 启动线程
    void start();

    // 获取线程id
    int getId() const;

private:
    ThreadFunc func_;
    static int generateId_;//类的静态变量用于给线程对象自定义id
    int threadId_; // 保存自定义的线程id
};

#endif