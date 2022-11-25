两个版本:1.基于C++11多线程编程实现的线程池，C++17的any类型和C++20的信号量semaphore自己用代码实现

2.使用packaged_task，future，可变参模板编程等等重构精简版本



## 功能点

1.基于master-slave线程模型，支持fixed和cached模式的线程池，能应用于高并发网络服务器或耗时任务处理

2.基于条件变量condition_variable和互斥锁mutex实现任务提交线程和任务执行线程间的通信机制

3.使用智能指针unique_ptr和shared_ptr管理ThreadPool中的线程对象和任务对象，防止内存泄漏 方便代码管理

4.基于基类指针和模板派生类实现Any，能保存任意任务的返回值

5.自己实现Semaphore，实现用户线程的Result对象和线程池中线程Task对象进行线程通信

6.精简版本中使用packaged_task和future代替了Result,Semaphore,Any，使用可变参模板编程代替了任务抽象基类Task





## Any类实现

基类的指针/引用能指向任意派生类对象，派生类是类模板去接收用户任务执行完的返回值

对于任意的数据类型T data，用Base*保存派生类对象Derive\<T>(data)

unique_ptr\<Base> base_=make_unique<Derive\<T>>(data) 用智能指针管理new出来的对象方便代码管理

对于获取真正的数据，dynamic_cast支持RTTI类型的转化，将基类指针转化为派生类指针并顺利拿到data

~~~c++
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
~~~







## Task抽象基类

用户应该继承Task基类并重写run方法

在ThreadPool中Task对象被强智能指针管理queue<shared_ptr\<Task>>，任务执行完后出作用域Task析构

如果用户想获取任务的返回值，在ThreadPool::submitTask(std::shared_ptr\<Task> task)

return Result(task)，用户线程的Result对象再以强智能指针管理Task对象，Task对象也能引用Result对象，子线程将任务的返回值Any给到该Result对象，并且调用Result对象的信号量通知用户线程有任务值了

~~~c++
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
~~~



~~~c++
Task::Task()
    : result_(nullptr)
{
}

//基类的方法 执行run方法并且获取任务执行完返回的Any
void Task::exec()
{
    // 子线程访问Task对象对应的Result对象
    if (result_ != nullptr)
    {
        result_->setVal(run()); //调用的是派生类的run方法
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
~~~





## Result结果类

用户线程中调用在ThreadPool::submitTask(std::shared_ptr\<Task> task) return Result(task)

提供setVal方法由子线程调用 给这个Result对象返回任务执行结果Any，Semaphore.post()

提供get方法由用户调用 Semaphore.wait()阻塞用户线程 返回Any

注意:Task对象在子线程中以强智能指针管理，任务执行完 调用对应Result对象的信号量方法后 才析构

而用户线程中的Result对象可能会在子线程调用Result对象的信号量方法之前析构，所以在访问Result对象的Semaphore之前应该访问设置标志位，以免访问已经析构的资源

~~~c++
class Result
{
public:
    Result(std::shared_ptr<Task> task, bool isValid = true);
    ~Result() = default;

    void setVal(Any any);// 子线程执行

    Any get();// 用户调用执行

private:
    Any any_;                    // 存储任务的返回值
    Semaphore sem_;              // 线程通信信号量
    std::shared_ptr<Task> task_; //指向对应获取返回值的任务对象
    std::atomic_bool isValid_;   // 返回值是否有效
};
~~~



~~~c++
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
~~~





## Semaphore信号量类

用一把mutex和一个condition_variable实现信号量实现线程间通信

对信号量来说，只要资源计数大于0，就是可以重入的

只要子线程把任务结果返回了，Result对象任意线程都能访问

~~~c++
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
~~~

~~~c++
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
~~~







## 线程包装类Thread

线程包装类 保存要执行的函数指针,在适当的时候实例化函数对象开启线程

~~~c++
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
~~~



~~~c++
int Thread::generateId_ = 0; //类的静态变量初始化

Thread::Thread(ThreadFunc func)
    : func_(func), threadId_(generateId_++)
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
~~~





## ThreadPool线程池类

1.在线程池开启时，怎么构造线程包装对象的函数指针?

线程包装类的函数指针如下:

using ThreadFunc = std::function<void(int)>;//指向ThreadPool::threadFunc任务分配函数

要将某个函数初始化一类函数指针，对于任意的函数类型,我们可以用lambad表达式做个包装

\[p](int)->void{*(p);} 这个整体可以给一个<void(int)>的函数指针初始化



我们要在ThreadPool的成员函数中给线程包装对象传入成员函数threadFunc并且这个threadFunc还需要其他参数

使用bind(&ThreadPool::threadFunc,this,placeholders::_1)，对函数做个再封装



2.线程包装对象执行函数的过程

线程包装对象执行thread.start()->thread t(func,threadId_)->ThreadPool::threadFunc(int threadId\_)

在start函数中，线程包对象传入相关的参数实例化了局部thread对象，所以要将线程分离，线程执行的是ThreadPool的任务分配函数

所有线程即消费者 通过mutex和condition_variable 访问任务队列并阻塞在相关的条件变量上，等待用户线程即生产者的唤醒



3.用户作为生产者给线程池提交任务，线程对象作为消费者消费任务，线程间同步问题

场景:多生产 少消费 不是一生产一消费场景，因为任务队列不满，用户将能一直生产

~~~c++
void ThreadPool::threadFunc(int threadid)
{
	auto lastTime = std::chrono::high_resolution_clock().now();

	// 回收资源策略:执行完任务再退出
	while (true)
	{	// 先定义一个shared_ptr管理的Task对象，它将在解锁后执行任务
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
~~~



4.用户提交任务接口

~~~c++
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
~~~



5.线程池析构，资源回收策略:执行完所有任务再结束所有线程

子线程有两种状态:1等待任务队列不为空，2正在执行任务

如果先唤醒再加锁，执行完任务的线程加锁访问任务队列发现为空，并且此时isPoolRunning_=true，这个线程将wait等待唤醒，而线程池不能唤醒这个线程并且自身等待着线程map为空，造成死锁。子线程等待生产者的通知，主线程等待子线程析构

先加锁再唤醒等待的线程，能防止以上情况发生

~~~c++
ThreadPool::~ThreadPool()
{
	isPoolRunning_ = false;// atomic_bool

	std::unique_lock<std::mutex> lock(taskQueMtx_);
	notEmpty_.notify_all(); // 唤醒等待的线程
	exitCond_.wait(lock, [&]() -> bool
				   { return threads_.size() == 0; });
    // 退出的线程需要唤醒ThreadPool
}
~~~





## 精简版本的优化

1.可变参模板编程让ThreadPool::submitTask可以接收任意任务函数和任意数量的参数，替代了Task抽象基类

2.使用C++新特性packaged_task和future代替了Result,Semaphore,Any



主要是重写了ThreadPool::submitTask方法

可变参模板编程+引用折叠+forword类型完美转发，无论传入什么类型，无论传入左值还是右值引用，bind对它们(函数对象和相关参数)做绑定，也就是再封装，packaged_task类似function定义了这类函数指针，再由make_shared去new这个对象并返回指向这个对象的shared_ptr，这个任务对象就定义好了



~~~c++
template <typename Func, typename... Args>
auto submitTask(Func &&func, Args &&...args) -> std::future<decltype(func(args...))>
{
    using RType = decltype(func(args...)); // decltype推导出函数返回类型并命名别名
    // 定义任务对象
    auto task = std::make_shared<std::packaged_task<RType()>>(
    std::bind(std::forward<Func>(func), std::forward<Args>(args)...));
    /*
    类似Result(Task)
    Result强智能指针管理Task对象，Task对象裸指针引用Result对象
    子线程找到Task对象对应的Result对象，使用Result对象的信号量做子线程和用户线程的通信
    任务执行的结果放在Result的Any对象中
    */
    std::future<RType> result = task->get_future();
    
    // 省略。。。
    
   	//对于提交任务失败，自定义空的任务对象并执行返回空的结果
    auto task = std::make_shared<std::packaged_task<RType()>>(
                []() -> RType
                { return RType(); });
            (*task)(); // 智能指针解引用得到函数指针并调用函数
            return task->get_future();
    
    /*
    提交成功 用户使用future::get()等待并得到任务返回结果
    类似Result，用户调用Result对象的信号量阻塞用户线程等待任务执行完的返回Any
    对于Any，dynamic_cast支持RTTI类型的转化，将基类指针转化为派生类指针并顺利拿到真正的数据
    */
    return result;
}
~~~



























































































