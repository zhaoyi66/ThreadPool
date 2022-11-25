#include "ThreadPool.h"
#include <thread>

//自定义任务类 继承Task 重写run方法
class MyTask : public Task
{
public:
    MyTask(int begin, int end)
        : begin_(begin), end_(end)
    {
    }

    Any run()
    {
        std::this_thread::sleep_for(std::chrono::seconds(2));
        return begin_ + end_;
    }

private:
    int begin_;
    int end_;
};

int main()
{
    ThreadPool pool;
    pool.setMode(PoolMode::MODE_CACHED);
    // 开始启动线程池
    pool.start(2);
    Result res1 = pool.submitTask(std::make_shared<MyTask>(1, 1));
    Result res2 = pool.submitTask(std::make_shared<MyTask>(2, 2));
    Result res3 = pool.submitTask(std::make_shared<MyTask>(3, 3));
    Result res4 = pool.submitTask(std::make_shared<MyTask>(4, 4));

    std::cout << res1.get().cast_<int>() << "," <<res2.get().cast_<int>() << std::endl;

    getchar();
    return 0;
}