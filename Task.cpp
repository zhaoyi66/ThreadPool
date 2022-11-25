#include "Task.h"

Task::Task()
    : result_(nullptr)
{
}

//基类的方法 执行run方法并且获取任务执行完返回的Any
void Task::exec()
{
    //在ThreadPool::submitTask return Result(Task)
    if (result_ != nullptr)
    {
        result_->setVal(run()); //子线程，调用的是派生类的run方法
    }
    else //用户不一定需要Result对象
    {
        run();
    }
}
void Task::setResult(Result *res)//在Result的构造函数中调用
{
    result_ = res;
}
