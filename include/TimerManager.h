#ifndef TIMERMANAGER_H
#define TIMERMANAGER_H

#include "TimeStamp.h"
#include <vector>
#include <queue>
#include <functional>
#include <memory>
#include <mutex>

// 定时器节点
class TimerNode {
public:
    using TimerCallback = std::function<void()>;

    TimerNode(TimeStamp expireTime, TimerCallback cb, int64_t intervalMs)
        : expireTime_(expireTime),
          callback_(std::move(cb)),
          intervalMs_(intervalMs),
          repeat_(intervalMs_ > 0) {}

    // 获取过期时间
    TimeStamp expireTime() const { return expireTime_; }
    // 执行回调
    void run() const { if (callback_) callback_(); }
    // 是否是重复定时器
    bool repeat() const { return repeat_; }
    // 重置过期时间
    void reset(TimeStamp now);

private:
    TimeStamp expireTime_;    // 过期时间
    TimerCallback callback_;  // 超时回调
    int64_t intervalMs_;      // 重复间隔（0表示不重复）
    bool repeat_;             // 是否重复
};

// 定时器管理器
class TimerManager {
public:
    using TimerPtr = std::shared_ptr<TimerNode>;

    TimerManager() = default;
    ~TimerManager() = default;

    // 添加定时器
    // delayMs: 延迟多少毫秒执行
    // cb: 超时回调
    // intervalMs: 重复间隔（0表示只执行一次）
    TimerPtr addTimer(int64_t delayMs, TimerNode::TimerCallback cb, int64_t intervalMs = 0);

    // 移除定时器
    void removeTimer(TimerPtr timer);

    // 获取最近一个定时器的超时时间（毫秒）
    // 返回-1表示没有定时器
    int getNextExpireTimeMs() const;

    // 处理所有超时的定时器
    void handleExpiredTimers();
    
private:
    // 比较器：用于最小堆（过期时间早的排在前面）
    struct TimerComparator {
        bool operator()(const TimerPtr& a, const TimerPtr& b) const {
            return a->expireTime() > b->expireTime();
        }
    };

    // 最小堆：存储所有定时器
    std::priority_queue<TimerPtr, std::vector<TimerPtr>, TimerComparator> timerHeap_;
    // 互斥锁：保证线程安全
    mutable std::mutex mutex_;
};

#endif // TIMERMANAGER_H