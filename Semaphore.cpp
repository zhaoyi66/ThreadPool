#include "Semaphore.h"

Semaphore::Semaphore(int limit)
    : resLimit_(limit)
    , isExit_(false)
{
}

Semaphore::~Semaphore()
{
    isExit_=true;
}

// 获取一个信号量资源
void Semaphore::wait()
{
    if(isExit_)
        return;

    std::unique_lock<std::mutex> lock(mtx_);
    cond_.wait(lock, [&]() -> bool
               { return resLimit_ > 0; });
    resLimit_--;
}
//增加一个信号量资源
void Semaphore::post()
{
    // 子线程访问用户线程的Result的Semaphore对象
    if(isExit_)
        return;
    std::unique_lock<std::mutex> lock(mtx_);
    resLimit_++;
    cond_.notify_all();
}
