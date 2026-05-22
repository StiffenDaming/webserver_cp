#include "EventLoop.h"
#include "Channel.h"
#include <iostream>
#include <cassert>

// 构造函数：初始化Epoll，默认不退出
// std::make_unique 是 C++14 引入的安全创建 unique_ptr 的方式，
// 它一次分配内存并构造对象，比 new Epoll 更安全（避免因异常导致的泄漏）。
// std::make_unique<Epoll>()是一个函数模板，注意它有括号
EventLoop::EventLoop()
    : epoll_(std::make_unique<Epoll>() ),
      timerManager_(std::make_unique<TimerManager>()), // 加上这一行
      quit_(false)
{
}

// 【核心】事件循环
void EventLoop::loop() {
    std::cout << "=== EventLoop 启动，开始监听事件 ===" << std::endl;

    // 死循环，直到quit_被设为true
    while (!quit_) {
        // 1. 获取最近一个定时器的超时时间
        int timeoutMs = timerManager_->getNextExpireTimeMs();

        // 2. 调用Epoll::wait，阻塞等待事件（超时-1表示永久阻塞）
        activeChannels_ = epoll_->wait(-1);

        // 3. 遍历所有活跃Channel，处理事件
        for (Channel* channel : activeChannels_) {
            // 调用Channel的handleEvent，执行回调
            channel->handleEvent(TimeStamp::now());
        }

        // 4. 处理所有超时的定时器
        timerManager_->handleExpiredTimers();
    }

    std::cout << "=== EventLoop 退出 ===" << std::endl;
}

// 退出循环
void EventLoop::quit() {
    quit_ = true;   // 原子写，其他线程的循环能立即看到
}

// 更新Channel到Epoll（Channel调用）
// 底层操作:epoll_ctl(ADD/MOD)
void EventLoop::updateChannel(Channel* channel) {
    // 断言：在调试模式下，如果 channel 为空指针，程序会立即终止并输出错误位置。这是用于捕获编程错误（不应该传入空指针）的安全检查。
    // 在 Release 模式下，assert 通常会被预处理移除，不影响性能。
    assert(channel != nullptr);

    epoll_->updateChannel(channel);
}

// 从Epoll移除Channel
// 底层操作:epoll_ctl(DEL)
void EventLoop::removeChannel(Channel* channel) {
    assert(channel != nullptr);
    epoll_->removeChannel(channel);
}

// 添加定时器接口（供外部调用）
void EventLoop::runAfter(int64_t delayMs, TimerNode::TimerCallback cb) {
    timerManager_->addTimer(delayMs, std::move(cb));
}

// 在EventLoop线程中执行任务
void EventLoop::runInLoop(std::function<void()> task) {
    if (isInLoopThread()) {
        task();
    } else {
        // 这里简化实现：直接在当前线程执行
        // 完整实现需要添加任务队列和唤醒机制
        task();
    }
}

// 判断是否在EventLoop线程
bool EventLoop::isInLoopThread() const {
    // 简化实现：假设EventLoop在主线程运行
    return true;
}

// 断言在EventLoop线程
void EventLoop::assertInLoopThread() const {
    // 简化实现
}