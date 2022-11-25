#ifndef _TASK_H_
#define _TASK_H_

#include <iostream>
#include <thread>

#include "Result.h"
/*
任务的抽象基类
*/
class Task
{
public:
    Task();
    ~Task() = default;

    //运行线程函数并获取返回值
    void exec();
    void setResult(Result *res);

    //从Task继承，重写run方法，实现自定义任务处理
    virtual Any run() = 0;

private:
    // Result::std::shared_ptr<Task> task_ 避免强智能指针的交叉引用
    Result *result_;
};

#endif