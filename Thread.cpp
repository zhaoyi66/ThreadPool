#include "Thread.h"
int Thread::generateId_ = 0; //类的静态变量初始化 唯一一个，对象共享

Thread::Thread(ThreadFunc func)
    : func_(func), threadId_(generateId_++) // Thread是ThreadPool一个个创建的
{
}

Thread::~Thread() {}

// 执行任务
void Thread::start()
{
    //定义线程对象并开启线程
    std::thread t(func_,threadId_);//给ThreadPool::threadFunc传入id 用于从ThreadPool::map中删除
    t.detach(); // t是局部对象 出作用域就析构 所以线程需要分离
}

int Thread::getId()const
{
	return threadId_;
}