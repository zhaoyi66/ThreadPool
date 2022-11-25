#include "ThreadPool.h"

const int TASK_MAX_THRESHHOLD = 64;	  //任务数量上限阈值
const int THREAD_MAX_THRESHHOLD = 10; //线程数量上限阈值 在cached模式下有用
const int THREAD_MAX_IDLE_TIME = 60;  //秒 在cached模式下 线程空闲时间超过就应该回收
const int THREAD_INSPECT_TIME = 5;	  //在cached模式 任务队列为空情况下 每间隔一段时间检查线程空闲时间

ThreadPool::ThreadPool()
	: initThreadSize_(0), taskSize_(0), idleThreadSize_(0), curThreadSize_(0), taskQueMaxThreshHold_(TASK_MAX_THRESHHOLD), threadSizeThreshHold_(THREAD_MAX_THRESHHOLD), poolMode_(PoolMode::MODE_FIXED), isPoolRunning_(false)
{
}

ThreadPool::~ThreadPool()
{
	isPoolRunning_ = false;

	std::unique_lock<std::mutex> lock(taskQueMtx_);
	notEmpty_.notify_all(); // 唤醒等待的线程
	exitCond_.wait(lock, [&]() -> bool
				   { return threads_.size() == 0; });
}

// 线程池状态
bool ThreadPool::checkRunningState() const
{
	return isPoolRunning_;
}

// 设置线程池的工作模式
void ThreadPool::setMode(PoolMode mode)
{
	if (checkRunningState()) //开启后不应该设置
		return;
	poolMode_ = mode;
}

// 设置任务数量上限阈值
void ThreadPool::setTaskQueMaxThreshHold(int threshhold)
{
	if (checkRunningState())
		return;
	taskQueMaxThreshHold_ = threshhold;
}

// 设置线程池cached模式下线程阈值
void ThreadPool::setThreadSizeThreshHold(int threshhold)
{
	if (checkRunningState())
		return;
	if (poolMode_ == PoolMode::MODE_CACHED)
	{
		threadSizeThreshHold_ = threshhold;
	}
}

// 开启线程池
void ThreadPool::start(int initThreadSize)
{
	// 设置线程池的运行状态
	isPoolRunning_ = true;

	// 记录初始线程个数
	initThreadSize_ = initThreadSize;
	curThreadSize_ = initThreadSize; //当前线程数量

	// 记录初始线程个数
	initThreadSize_ = initThreadSize;

	// 创建线程对象
	for (int i = 0; i < initThreadSize_; i++)
	{
		//创建thread线程对象 把线程分配函数 给到 线程包装类
		auto ptr = std::make_unique<Thread>(std::bind(&ThreadPool::threadFunc, this, std::placeholders::_1));
		int threadId = ptr->getId();
		threads_.emplace(threadId, std::move(ptr)); // unique_ptr右值引用构造 转移资源所有权
	}

	for (int i = 0; i < initThreadSize_; i++)
	{
		//执行线程函数
		threads_[i]->start(); // thread.start()->thread::func_()->ThreadPool::threadFunc()
		idleThreadSize_++;	  // 记录初始空闲线程的数量
	}
}

// 任务分配函数  线程池的所有线程从任务队列里面分配并消费任务
void ThreadPool::threadFunc(int threadid)
{
	auto lastTime = std::chrono::high_resolution_clock().now();

	// 回收资源策略:执行完任务再退出
	while (true)
	{
		std::shared_ptr<Task> task;
		{
			//加锁访问任务队列
			std::unique_lock<std::mutex> lock(taskQueMtx_);

			std::cout << "tid:" << std::this_thread::get_id()
					  << "尝试获取任务..." << std::endl;

			while (taskQue_.size() == 0)
			{
				if (!isPoolRunning_)
				{
					// isPoolRunning_==flase
					threads_.erase(threadid);
					std::cout << "threadid:" << std::this_thread::get_id() << " exit!"
							  << std::endl;
					exitCond_.notify_all();
					return; //线程结束
				}

				/*
				在cached模式下多创建的线程
				空闲时间超过THREAD_MAX_IDLE_TIME并且线程数量超过initThreadSize_就应该回收
				*/
				if (poolMode_ == PoolMode::MODE_CACHED)
				{
					// 在THREAD_INSPECT_TIME秒内生产者没有唤醒该消费者 wait_for会返回timeout 检查该线程空闲时间
					if (std::cv_status::timeout == notEmpty_.wait_for(lock, std::chrono::seconds(THREAD_INSPECT_TIME)))
					{
						// 计算该线程最后一次执行任务到现在的时间间隔
						auto now = std::chrono::high_resolution_clock().now();
						auto dur = std::chrono::duration_cast<std::chrono::seconds>(now - lastTime);

						if (dur.count() >= THREAD_MAX_IDLE_TIME && curThreadSize_ > initThreadSize_)
						{
							// 退出线程函数 线程销毁
							// 从ThreapPool::map中移出
							threads_.erase(threadid); // threadid不是真正的线程ID
							curThreadSize_--;
							idleThreadSize_--;

							std::cout << "threadid:" << std::this_thread::get_id() << " exit!"
									  << std::endl;
						}
					}
				}
				else
				{
					// 在fixed模式下 任务队列为空就等待不空通知
					notEmpty_.wait(lock);
				}
			}

			std::cout << "tid:" << std::this_thread::get_id()
					  << "获取任务成功..." << std::endl;

			// taskQue_.size() != 0
			idleThreadSize_--;
			task = taskQue_.front();
			taskQue_.pop(); // Task移出任务队列
			taskSize_--;

			notFull_.notify_all(); //通知生产者生产
		}						   //出作用域智能指针析构 解锁

		//线程执行任务
		if (task != nullptr)
		{
			task->exec();
		}

		idleThreadSize_++;
		lastTime = std::chrono::high_resolution_clock().now(); // 更新线程执行完任务的时间

	} //由于submitTask return  Result(Task) Task的生命周期延长 和Result一起析构
}

// 给线程池提交任务 用户调用该接口，传入任务对象
Result ThreadPool::submitTask(std::shared_ptr<Task> sp) //生产者
{
	// 获取锁 访问任务队列
	std::unique_lock<std::mutex> lock(taskQueMtx_);
	if (!notFull_.wait_for(lock, std::chrono::seconds(1),
						   [&]() -> bool
						   { return taskQue_.size() < (size_t)taskQueMaxThreshHold_; }))
	{
		// 表示notFull_等待1s种，条件依然没有满足
		std::cerr << "task queue is full, submit task fail." << std::endl;

		return Result(sp, false); //局部的sp生命周期延长到Result:: shared_ptr<Task>task_
	}

	taskQue_.emplace(sp);
	taskSize_++;

	notEmpty_.notify_all(); //任务队列不空通知

	/*
	cached模式 任务处理比较紧急 场景：小而快的任务
	需要根据任务数量和空闲线程的数量，判断是否需要创建新的线程出来
	*/
	if (poolMode_ == PoolMode::MODE_CACHED && taskSize_ > idleThreadSize_ && curThreadSize_ < threadSizeThreshHold_)
	{
		std::cout << ">>> create new thread..." << std::endl;

		// 创建新的线程对象
		auto ptr = std::make_unique<Thread>(std::bind(&ThreadPool::threadFunc, this, std::placeholders::_1));
		int threadId = ptr->getId();
		threads_.emplace(threadId, std::move(ptr));
		// 启动线程
		threads_[threadId]->start();
		// 修改线程个数相关的变量
		curThreadSize_++;
		idleThreadSize_++;
	}

	return Result(sp); //匹配右值引用的构造
}
