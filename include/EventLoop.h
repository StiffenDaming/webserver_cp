#ifndef EVENTLOOP_H
#define EVENTLOOP_H

#include "Epoll.h"
#include "TimerManager.h" // 加上这一行
#include <vector>
#include <atomic>
#include <memory>

// 前向声明（和Channel互相依赖）
class Channel;

// EventLoop：Reactor 模式的核心事件循环
// 一个线程一个EventLoop（one loop per thread）
class EventLoop {
public:
    EventLoop();
    ~EventLoop() = default;

    // 【核心】启动事件循环（死循环，直到quit）
    void loop();

    // 退出事件循环
    void quit();

    // 供Channel调用：更新/添加Channel到epoll
    void updateChannel(Channel* channel);

    // 从epoll移除Channel
    void removeChannel(Channel* channel);

    // 添加定时器：延迟delayMs毫秒执行cb
    void runAfter(int64_t delayMs, TimerNode::TimerCallback cb);

    // 在EventLoop线程中执行任务
    void runInLoop(std::function<void()> task);

    // 判断是否在EventLoop线程
    bool isInLoopThread() const;
    // 断言在EventLoop线程
    void assertInLoopThread() const;
    
private:
    // 为什么用 std::unique_ptr 而不是原始指针或普通对象？
    // 独占所有权：EventLoop 独自拥有这个 Epoll 对象，不允许其他对象共享或接管。
    // 自动释放内存：当 EventLoop 对象被销毁时，unique_ptr 的析构函数会自动 delete 内部的 Epoll 对象，避免手动调用 delete 导致的内存泄漏。
    // 异常安全：如果在构造 EventLoop 过程中抛出异常，已创建的 Epoll 对象也能被正确清理。
    // 明确语义：使用 unique_ptr 清晰表达了“这个 Epoll 只属于我，且由我全权负责释放”。
    
    // 持有Epoll实例（核心IO复用器）
    // 这是一个类模板 ，unique_ptr<Epoll> 类比于vector<int> ，是一个类型
    std::unique_ptr<Epoll> epoll_;

    // 为什么用 std::atomic<bool> 而不是普通的 bool？
    // 多线程安全：
    // 通常 EventLoop 运行在某个线程（比如 I/O 线程），而 quit() 函数可能被另一个线程（比如主线程或信号处理线程）调用。
    // 如果一个线程修改 quit_（写操作），另一个线程循环读取 quit_，普通 bool 无法保证：
        // 可见性：写操作可能只停留在 CPU 缓存中，读线程永远看不到新值。
        // 数据竞争：并发读写非原子变量是未定义行为（可能导致程序崩溃或死循环）。
    // 原子操作保证：
    // std::atomic<bool> 确保对 quit_ 的读写是原子的，并且具有适当的内存序（默认是 memory_order_seq_cst），保证修改能立即被其他线程看见。

    // 退出标志（原子变量，线程安全）,[əˈtɒmɪk]
    std::atomic<bool> quit_;

    // 存储epoll返回的活跃Channel
    std::vector<Channel*> activeChannels_;

    std::unique_ptr<TimerManager> timerManager_; // 加上这一行
};

#endif // EVENTLOOP_H