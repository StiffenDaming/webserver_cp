#ifndef EPOLL_H
#define EPOLL_H

#include "Channel.h"
#include <vector>
#include <sys/epoll.h>

// Epoll 类：封装 Linux epoll 系统调用
// 负责：创建 epoll → 添加/修改/删除 fd → 等待事件
class Epoll {
public:
    Epoll();
    ~Epoll();

    // 核心：等待事件发生，返回有事件的 Channel 列表
    // 整体：这个函数调用 epoll_wait 等待事件发生，然后返回一个装有 有事件发生的 Channel 指针 的容器。
    std::vector<Channel*> wait(int timeoutMs = -1);
    // 可以把 epoll_wait 想象成一个“事件通知器”：
    // 你事先告诉内核：“帮我监控这些 fd，我对它们的读/写事件感兴趣”。
    // 然后你调用 epoll_wait，内核说：“好的，你先等着，有动静我通知你”。
    // 当某个 fd 真的发生了事件，内核把你叫醒，并告诉你：“这是发生事件的 fd 列表，以及每个 fd 具体发生了什么”。
    // 你拿到列表后，遍历每个 fd，找到对应的 Channel，然后调用它的 handleEvent 来处理。

    // 把 Channel 注册/更新到 epoll
    void updateChannel(Channel* channel);

    // 从 epoll 移除 Channel
    void removeChannel(Channel* channel);

private:
    // 执行底层 epoll_ctl 操作
    void update(int operation, Channel* channel);

    // epoll 最大监听事件数
    static const int kMaxEvents = 1024;

    // epoll 文件描述符
    int epollFd_;

    // 存储 epoll_wait 返回的事件
    // epoll_event 是 Linux 系统头文件 <sys/epoll.h> 中定义的结构体，用于描述 epoll 事件。
    std::vector<struct epoll_event> events_;
    // epoll_event 结构体定义:
    // struct epoll_event {
    // uint32_t events;   // 事件掩码（EPOLLIN, EPOLLOUT 等）
    // epoll_data_t data; // 用户数据，通常存 fd 或 Channel 指针
    // };
    // typedef union epoll_data {
    //     void *ptr;
    //     int fd;
    //     uint32_t u32;
    //     uint64_t u64;
    // } epoll_data_t;

};

#endif