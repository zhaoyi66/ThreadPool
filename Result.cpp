#include "Result.h"
#include"Task.h"

Result::Result(std::shared_ptr<Task> task, bool isValid)
    : isValid_(isValid), task_(task)
{
    task_->setResult(this);
}

void Result::setVal(Any any) //子线程执行
{
    //存储task的返回值
    this->any_=std::move(any);
    this->sem_.post();//已经获取了任务执行的返回值 增加信号量资源
}

Any Result::get() //用户调用 获取任务执行完的结果Any
{
    if (!isValid_)
    {
        return "";
    }
    sem_.wait(); //阻塞用户的线程 等待线程执行完任务
    return std::move(any_);//unique_ptr只用右值引用的
}
