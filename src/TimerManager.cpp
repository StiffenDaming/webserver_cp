#include "TimerManager.h"

// 重置定时器过期时间
void TimerNode::reset(TimeStamp now) {
    expireTime_ = TimeStamp(now.microSecondsSinceEpoch() + intervalMs_ * 1000);
}

// 添加定时器
TimerManager::TimerPtr TimerManager::addTimer(int64_t delayMs, TimerNode::TimerCallback cb, int64_t intervalMs) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    TimeStamp now = TimeStamp::now();
    TimeStamp expireTime(now.microSecondsSinceEpoch() + delayMs * 1000);
    TimerPtr timer = std::make_shared<TimerNode>(expireTime, std::move(cb), intervalMs);
    timerHeap_.push(timer);
    
    return timer;
}

// 移除定时器（标记为无效，实际在handleExpiredTimers时清理）
void TimerManager::removeTimer(TimerPtr timer) {
    // 简单实现：不立即从堆中删除，因为堆不支持随机删除
    // 超时检查时会自动忽略无效的定时器
}

// 获取最近一个定时器的超时时间
int TimerManager::getNextExpireTimeMs() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (timerHeap_.empty()) {
        return -1; // 没有定时器，epoll_wait永久阻塞
    }

    TimeStamp now = TimeStamp::now();
    int64_t diff = timerHeap_.top()->expireTime().microSecondsSinceEpoch() - now.microSecondsSinceEpoch();
    
    if (diff < 0) {
        return 0; // 已经超时，立即返回
    } else {
        return static_cast<int>(diff / 1000); // 转换为毫秒
    }
}

// 处理所有超时的定时器
void TimerManager::handleExpiredTimers() {
    std::lock_guard<std::mutex> lock(mutex_);
    TimeStamp now = TimeStamp::now();

    // 遍历堆顶，处理所有超时的定时器
    while (!timerHeap_.empty()) {
        TimerPtr timer = timerHeap_.top();
        
        if (timer->expireTime() > now) {
            break; // 堆顶都没超时，后面的都没超时
        }

        // 执行回调
        timer->run();

        // 弹出堆顶
        timerHeap_.pop();

        // 如果是重复定时器，重新加入堆
        if (timer->repeat()) {
            timer->reset(now);
            timerHeap_.push(timer);
        }
    }
}