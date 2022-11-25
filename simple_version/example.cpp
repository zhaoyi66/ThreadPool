#include <iostream>
#include <functional>
#include <thread>
#include <future>
#include <chrono>
using namespace std;

#include "threadpool.h"

/*
优化:
1.使用可变参模板编程让线程池提交任务更加方便
pool.submitTask(sum1, 10, 20);
pool.submitTask([](int a,int b)->int{return a+b;}, 10, 20);

2.使用packaged_task和future代替了Result节省线程池代码
*/
int sum(int a, int b)
{
    this_thread::sleep_for(chrono::seconds(2));
    return a + b;
}
// io线程
void io_thread(int listenfd)
{
}
// worker线程
void worker_thread(int clientfd)
{
}

int main()
{
    {
        ThreadPool pool;
        pool.setMode(PoolMode::MODE_CACHED);
        pool.start(2);

        future<int> t1 = pool.submitTask(sum, 1, 2);
        future<int> t2 = pool.submitTask([](int a, int b) -> int
                                         { return a + b; },
                                         2, 3);
        future<int> t3 = pool.submitTask(sum, 3, 3);
        future<int> t4 = pool.submitTask(sum, 4, 4);

        cout << t1.get() << endl;
        cout << t2.get() << endl;
    }
    return 0;
}
