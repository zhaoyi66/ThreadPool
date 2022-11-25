#ifndef _THREADPOOL_H
#define _THREADPOOL_H

#include <vector>
#include <memory>
#include <atomic>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <unordered_map>
#include <iostream>

#include "Thread.h"
#include "Result.h"
#include "Task.h"
#include "Semaphore.h"

/*
Master - Slave线程模型
Master线程给Slave线程分配任务
等待Slave线程执行完任务返回结果
Master线程合并各个任务结果
    支持fixed模式和cached模式
*/

enum class PoolMode
{
    MODE_FIXED,  // 固定数量的线程
    MODE_CACHED, // 线程数量可动态增长
};

// 线程池类型
class ThreadPool
{
public:
    ThreadPool();
    ~ThreadPool();

    // 设置线程池的工作模式
    void setMode(PoolMode mode);

    // 设置task任务队列上线阈值
    void setTaskQueMaxThreshHold(int threshhold);

    // 设置线程池cached模式下线程阈值
    void setThreadSizeThreshHold(int threshhold);

    //只允许对象实例化
    ThreadPool(const ThreadPool &) = delete;
    ThreadPool &operator=(const ThreadPool &) = delete;

    // 给线程池提交任务
    Result submitTask(std::shared_ptr<Task> sp); //用户应该提交new出来任务对象

    // 开启线程池
    void start(int initThreadSize = std::thread::hardware_concurrency());

private:
    // 任务分配函数
    void threadFunc(int threadid);

    // 检查pool的运行状态
    bool checkRunningState() const;

    std::unordered_map<int, std::unique_ptr<Thread>> threads_; // 线程列表

    int initThreadSize_;             // 初始的线程数量
    int threadSizeThreshHold_;       // 线程数量上限阈值 在cached模式下有用
    std::atomic_int curThreadSize_;  // 记录当前线程池里面线程的总数量
    std::atomic_int idleThreadSize_; // 记录空闲线程的数量

    // shared_ptr方便执行完任务后析构
    std::queue<std::shared_ptr<Task>> taskQue_; //任务队列
    std::atomic_int taskSize_;                  // 任务的数量
    int taskQueMaxThreshHold_;                  // 任务队列数量上限阈值

    std::mutex taskQueMtx_;            // 保证任务队列的线程安全
    std::condition_variable notFull_;  // 表示任务队列不满
    std::condition_variable notEmpty_; // 表示任务队列不空
    std::condition_variable exitCond_; // 等到线程资源全部回收

    PoolMode poolMode_;              //当前线程池的工作模式
    std::atomic_bool isPoolRunning_; // 表示当前线程池的启动状态
};

#endif