#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <vector>                // std::vector,emplace_back 函数
#include <queue>
#include <thread>                // std::thread,hardware_concurrency 函数
#include <mutex>                 // std::mutex, std::lock_guard, std::unique_lock
#include <condition_variable>    // std::condition_variable
#include <functional>
#include <atomic>

class ThreadPool {
public:
    using Task = std::function<void()>;

    // 构造函数：指定线程池大小
    // explicit：防止隐式类型转换，例如 ThreadPool pool = 4; 不被允许，必须显式写 ThreadPool pool(4);。避免意外的整数转对象。
    // 默认参数 = std::thread::hardware_concurrency()：如果没有传入参数，线程数自动设为当前机器的 CPU 核心数（通常用于最佳性能）。例如 4 核 8 线程的 CPU，该函数可能返回 8 或 4，取决于实现（传统上返回硬件支持的并发线程数，如超线程的线程数）。
    explicit ThreadPool(size_t threadNum = std::thread::hardware_concurrency());
    ~ThreadPool();

    // 向线程池添加任务
    void addTask(Task task);

    // 停止线程池
    void stop();

private:
    // 工作线程函数
    // 这个 workerThread 实现了非常标准的 “生产者-消费者”模式：
    // 生产者：调用 addTask 的线程，将任务入队，并 notify_one 唤醒一个工作线程。
    // 消费者：工作线程循环等待任务，执行后继续等待。
    void workerThread();

    std::vector<std::thread> threads_;          // 工作线程数组
    std::queue<Task> taskQueue_;                // 任务队列

    // mutable [ˈmju:təbl]的中文意思是“可变的，易变的”，跟constant（既C++中的const）是反义词。
    // 在C++中，mutable也是为了突破const的限制而设置的。被mutable修饰的变量，将永远处于可变的状态，即使在一个const函数中.
    // mutable它可以突破const的限制，在被const修饰的函数里面也能被修改
    mutable std::mutex mutex_;                  // 互斥锁,[m'ju:teks]

    // condition_variable 常用成员函数:
    // - wait：阻塞当前线程直到条件变量被唤醒
    // - notify_one：通知一个正在等待的线程
    // - notify_all：通知所有正在等待的线程
    // 使用 wait 必须搭配 std::unique_lock<std::mutex> 一起使用
    std::condition_variable condition_;         // 条件变量
    std::atomic<bool> stop_;                    // 是否停止线程池
};

#endif // THREADPOOL_H