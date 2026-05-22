#include "ThreadPool.h"
#include <iostream>

// 构造函数：创建指定数量的工作线程
ThreadPool::ThreadPool(size_t threadNum)
    : stop_(false)
{
    // 循环创建 threadNum 个工作线程
    for (size_t i = 0; i < threadNum; ++i) {

        // 这个语句意思是：创建一个新的线程，让它执行当前 ThreadPool 对象的 workerThread 成员函数，并将这个线程对象添加到 threads_ 容器末尾。
        // 当使用成员函数作为线程入口时，必须提供一个该类的对象指针（即 this），以便在调用时绑定到该对象上。
        // 实际效果等价于：在新线程中执行 (this->*workerThread)()，即调用当前 ThreadPool 对象的 workerThread 成员函数。
        // 每个线程启动后，会立即进入 workerThread 函数，并在其中循环等待任务。
        threads_.emplace_back(&ThreadPool::workerThread, this);
            // 为什么不用 push_back？
        // push_back 需要先构造一个临时 std::thread 对象再拷贝/移动到容器中，而 emplace_back 直接在容器内存中构造，效率更高，且避免了 std::thread 不可拷贝的问题。
            // 线程何时开始运行？
        // std::thread 对象一旦构造完成，新线程就会立即启动并执行入口函数。
            // 如何保证线程安全？
        // workerThread 内部使用互斥锁和条件变量，保护任务队列的访问。

    }
    std::cout << "✅ 线程池启动，工作线程数：" << threadNum << std::endl;
}

// 析构函数：停止线程池
ThreadPool::~ThreadPool() {
    if (!stop_) {
        // 当线程池析构时，调用 stop() 设置 stop_ = true，然后 notify_all() 唤醒所有线程，
        // 它们会检查条件发现 stop_ == true 且队列为空后逐个退出，
        // 最后主线程调用 join() 等待所有线程结束。
        stop();
    }
}

// 向线程池添加任务
void ThreadPool::addTask(Task task) {
    if (stop_) {
        return;
    }

    std::lock_guard<std::mutex> lock(mutex_);
    taskQueue_.push(std::move(task));
    condition_.notify_one(); // 唤醒一个等待的线程
}

// 停止线程池
void ThreadPool::stop() {
    stop_ = true;
    condition_.notify_all(); // 唤醒所有等待的线程

    // 等待所有线程退出
    for (auto& thread : threads_) {
        // 必须做选择：每个 std::thread 对象析构前必须 join() 或 detach()，否则程序崩溃。
        // 不能重复 join：调用前可用 joinable() 检查线程是否可以被 join
        if (thread.joinable()) {
            //join() 是 std::thread 的一个成员函数，作用是阻塞当前线程，
            //等待被调用 join() 的那个线程执行完毕，然后当前线程才继续往下执行。
            //一句话总结：join() 是让主线程“等一下”子线程完成，确保线程资源被正确回收
            thread.join();
        }
    }
    std::cout << "✅ 线程池已停止" << std::endl;
}

// 工作线程函数：不断从任务队列取任务执行
void ThreadPool::workerThread() {
    while (!stop_) {
        Task task;
        {
            // mutex_ 是保护任务队列的互斥锁。
            // std::unique_lock 比 std::lock_guard 更灵活，因为可以手动解锁（在 condition_.wait 内部会释放锁），也可以延迟加锁。
            // 这里直接加锁，保护后续对任务队列的访问。
            std::unique_lock<std::mutex> lock(mutex_);

            // 等待任务队列不为空或停止信号
            // wait 的第二个参数是一个谓词（lambda）
            // lambda [this] 捕获 this：因为需要访问成员变量 taskQueue_ 和 stop_，所以捕获 this 指针。
            condition_.wait(lock, [this]() {
                // 如果任务队列为不为空（队列中还有任务） 或者 线程池已停止，则退出
                // 反之，队列空且未停止，wait 会原子地释放 lock，并将当前线程阻塞，直到其他线程调用 condition_.notify_one() 或 condition_.notify_all()。
                // 当被唤醒后，会重新获得锁，并再次检查谓词。只有谓词（lambda）返回 true 才继续执行，否则再次阻塞。这避免了“虚假唤醒”。
                return !taskQueue_.empty() || stop_;
                // 情况	!taskQueue_.empty() (A)	stop_ (B)	A || B	线程行为	状态描述
                // 1	✅ true	✅ true	✅ true	唤醒后进入循环体，会取出任务执行	停止但还有任务：线程池已停止，但队列非空，线程继续处理剩余任务，直到队列清空后退出
                // 2	✅ true	❌ false	✅ true	唤醒后正常取任务执行	正常工作：队列有任务，线程立即执行
                // 3	❌ false	✅ true	✅ true	唤醒后检查 if (stop_ && taskQueue_.empty()) return;，条件成立，线程退出	停止且无任务：线程池已停止，队列为空，线程直接退出
                // 4	❌ false	❌ false	❌ false	线程继续阻塞在 wait() 上，等待被 notify_one() / notify_all() 唤醒	空闲等待：队列无任务，未停止，线程休眠，不消耗 CPU
                // 注意：情况1中即使 stop_ = true，线程也不会立即退出，而是先处理完队列中积压的任务，这通常是期望的行为（保证已提交的任务都被执行）。只有情况3（停止且队列空）线程才会退出。
            });

            // 如果线程池已停止 且 任务队列为空，则线程安全退出。
            if (stop_ && taskQueue_.empty()) {
                return;
            }

            // 从队列头部取出任务，使用 std::move 转移 std::function 的底层资源，避免拷贝（std::function 拷贝可能开销较大）。
            task = std::move(taskQueue_.front());
            taskQueue_.pop();   // pop 删除队列头部元素。

            // 注意：此时仍持有锁，直到离开当前代码块（unique_lock 析构解锁）或手动解锁。
        }   // 这里 lock 对象析构，自动解锁 mutex_

        // 执行任务
        // task 是 std::function<void()> 类型，可以隐式转换为 bool（检查是否为空）。
        if (task) {
            // 如果不为空，调用 task() 执行任务。任务通常是用户绑定的回调函数。
            task();
        }
    }
}