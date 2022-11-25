#ifndef _RESULT_H
#define _RESULT_H

#include <memory>
#include <atomic>

#include "Semaphore.h"
class Task;

/*
自定义C++17的Any类型：可以接收任意数据的类型
Any接受T data,基类指针能指向任意派生类对象
派生类是类模板，能保存任意数据类型
*/
class Any
{
public:
    // Any类型接收任意其它的数据
    template <typename T>
    Any(T data)
        : base_(std::make_unique<Derive<T>>(data))
    {
    }

    //把Any对象里面存储的data数据提取出来
    template <typename T>
    T cast_()
    {
        //基类指针->派生类指针  dynamic_cast支持RTTI上下转换
        Derive<T> *pd = dynamic_cast<Derive<T> *>(base_.get()); // get()从unique_ptr对象取出指针
        if (pd == nullptr)
        {
            throw "type is unmatch";
        }
        return pd->data_;
    }

    Any() = default;
    ~Any() = default;

    // unique_ptr delete左值引用的构造，赋值构造 保留右值引用的
    Any(const Any &) = delete;
    Any &operator=(const Any &) = delete;
    Any(Any &&) = default;
    Any &operator=(Any &&) = default;

private:
    // 基类类型
    class Base
    {
    public:
        /*
        如果不把基类的析构函数设置为虚函数，基类的指针/引用指向派生类的对象时，delete发生静态绑定
        调用基类的析构而不会调用派生类的析构
        基类是虚析构函数，派生类也会变虚析构函数。动态绑定
        */
        virtual ~Base() = default;
    };

    // 派生类类型
    template <typename T>
    class Derive : public Base
    {
    public:
        Derive(T data) : data_(data)
        {
        }
        T data_; //保存了任意的其它类型
    };

    // Any::unique_ptr<Base>
    std::unique_ptr<Base> base_; //基类指针指向派生类对象，而派生类是类模板 能保存所有类型的数据
};

// 实现接收提交到线程池的task任务执行完成后的返回值类型Result
//由于Any包含类模板 实现和声明放同一文件下
class Result
{
public:
    Result(std::shared_ptr<Task> task, bool isValid = true);
    ~Result() = default;

    void setVal(Any any);

    Any get();

private:
    Any any_;                    // 存储任务的返回值
    Semaphore sem_;              // 线程通信信号量
    std::shared_ptr<Task> task_; //指向对应获取返回值的任务对象
    std::atomic_bool isValid_;   // 返回值是否有效
};
#endif