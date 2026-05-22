#include "Epoll.h"
#include <unistd.h>
#include <cstring>
#include <iostream>

// 构造：创建 epoll 实例
//调用 epoll_create1 创建 epoll 实例，并设置 close-on-exec
Epoll::Epoll()
    : epollFd_(epoll_create1(EPOLL_CLOEXEC)),
      events_(kMaxEvents)
{
    // epollFd_ 在进入构造函数体之前就被初始化为 epoll_create1(...) 的返回值。
    // 如果 epoll_create1 成功，返回一个正数（新的 fd）；如果失败，返回 -1。
    if (epollFd_ < 0) {
        perror("epoll_create1 failed");
    }
}

// 析构：关闭 epollFd
Epoll::~Epoll() {
    // close 是 POSIX 标准函数，用于关闭一个文件描述符。
    close(epollFd_);
}

// 核心：等待事件，返回活跃 Channel
std::vector<Channel*> Epoll::wait(int timeoutMs) {
    // epoll_wait 的参数说明：
    // epollFd_: epoll 实例的文件描述符
    // events_.data(): 存储事件的数组
    // events_.size(): 数组的大小       
    // timeoutMs: 等待事件的超时时间，单位是毫秒。-1 表示无限等待。
    // epoll_wait 的返回值 n 表示有多少个事件被触发了（即有多少个 fd 发生了感兴趣的事件）。
    // 如果 n < 0，表示发生了错误；如果 n == 0，表示超时了没有事件发生；如果 n > 0，表示有 n 个事件被触发了。
    // 我们需要遍历这 n 个事件，找到对应的 Channel，并把它们放到 activeChannels 中返回。
    // 注意：events_ 是 Epoll 类的成员变量，是一个 vector<epoll_event>，我们在构造函数中已经初始化了它的大小为 kMaxEvents。
    int n = epoll_wait(epollFd_, events_.data(), events_.size(), timeoutMs);
    // events_ 是 Epoll 类的成员变量，在构造函数中已经分配了空间（events_(kMaxEvents)），但里面的内容是未初始化的。
    // epoll_wait 调用时，内核会将就绪事件的 epoll_event 结构体逐个填充到 events_.data() 指向的数组中。
        // events_的数据从哪来？
    // 在Epoll::updateChannel函数中，epoll_ctl已经把Channel类的fd和Epoll类的epollFd_关联起来了。
    // 之前调用 epoll_ctl(ADD/MOD) 时，已经把每个 fd 的监听事件掩码和对应的 Channel 指针（ev.data.ptr）拷贝到了内核中。
    // 现在，当某个 fd 上发生了注册的事件（如可读、可写），内核会将这个 fd 及其关联的 Channel 指针、实际发生的事件类型（如 EPOLLIN）填充到 events_ 数组中。
    // epoll_wait 返回就绪事件的个数 n，events_ 数组的前 n 个元素就是这些就绪事件的信息。
        // epoll_wait 是怎样“筛选出有用的 Channel”的？
    // epoll_wait 返回的只是那些 实际发生了注册事件 的 fd 对应的信息（即就绪事件）。
    // 内核根据之前通过 epoll_ctl 存储的 events 掩码和 data，把就绪事件的 events 和 data 填充到用户提供的 events_ 数组中。
    // 因此，只有那些状态活跃（有事件发生）的 Channel 才会被返回，没有事件发生的 Channel 不会被包含在返回列表中。
    // 这正是“筛选”的含义——epoll_wait 帮你从大量注册的 fd 中挑出当前需要处理的少数几个。
 
    std::vector<Channel*> activeChannels;

    // 遍历所有活跃的触发事件,从events_ 中取回内核返回有活跃的指针，事件
    for (int i = 0; i < n; ++i) {
        // events_[i].data.ptr 的类型是 void*。
        // 我们知道它实际指向一个 Channel 对象，因此用 static_cast<Channel*> 将它转回 Channel*。
        Channel* channel = static_cast<Channel*>(events_[i].data.ptr);  // 内核返回的一定是有效的指针
        
        // set_revents 的作用是将内核报告的事件类型（如 EPOLLIN、EPOLLOUT）保存到 Channel 的 revents_ 成员变量中。
        // Channel 只需要知道发生了什么事（读、写、错误等），而不需要知道 data 中的其他信息（因为 data.ptr 已经用来定位 Channel 本身了）。
        // 所以只需要传入 events_[i].events 这个整数掩码即可。
        channel->set_revents(events_[i].events); // 设置触发的事件

        activeChannels.push_back(channel);
    }

    return activeChannels;
}

// 更新 Channel 到 epoll
void Epoll::updateChannel(Channel* channel) {
    int fd = channel->fd();

    struct epoll_event ev{};
    // 初始化方式二，C风格:
    // memset 是 C 标准库函数，用于将一块内存区域（这里是 ev）设置为指定的值（这里是 0）。
    // memset(&ev, 0, sizeof(ev));

    //定义变量：在栈上声明一个 epoll_event 结构体变量 ev，用于临时构造要注册的事件。
    ev.events = channel->events();   // 从 Channel 获取事件掩码
    // 存储 Channel 指针，以便 epoll_wait 返回时知道是哪个 Channel
    ev.data.ptr = channel; 

    // 这段代码的逻辑是：
    // 先尝试修改（MOD）该 fd 的监听事件，如果修改失败（返回值 <0），
    // 则说明该 fd 尚未被添加到 epoll 中（因为只有已添加的 fd 才能被修改），
    // 于是再执行添加（ADD）操作。
    if (epoll_ctl(epollFd_, EPOLL_CTL_MOD, fd, &ev) < 0) 
    {
        // 如果还没加入过 → ADD
        // 如果已经加入 → MOD
        // 这是我们之前踩过坑的地方！

        // 注意：
        // epoll_ctl 调用时会将 ev 的内容（包括 events 和 data.ptr）复制到内核中，与 epollFd_ 和 fd 关联起来，所以之后 Epoll::wait.epoll_wait() 可以读出这些信息。
        epoll_ctl(epollFd_, EPOLL_CTL_ADD, fd, &ev);
        // int epoll_ctl(int epfd, int op, int fd, struct epoll_event *event);
        // 参数说明：
        // epfd：epoll 实例的文件描述符（epollFd_）。
        // op：操作类型，可选：
            // EPOLL_CTL_ADD：添加一个新的 fd 到监控列表。
            // EPOLL_CTL_MOD：修改已监控 fd 的事件类型。
            // EPOLL_CTL_DEL：删除一个 fd，此时 event 参数可以传 NULL。
        // fd：要操作的文件描述符（这里是 channel->fd()）。
        // event：指向 epoll_event 结构体的指针，&ev 告诉内核：监听 fd 的哪些事件，以及事件发生时返回 channel 指针。
        // 返回值：成功返回 0，失败返回 -1 并设置 errno。
    }
}

// 从 epoll 移除 Channel
void Epoll::removeChannel(Channel* channel) {
    int fd = channel->fd();
    // 删除操作只需要知道 fd，不需要提供事件信息，所以第四个参数可以传 NULL（或 nullptr）。
    epoll_ctl(epollFd_, EPOLL_CTL_DEL, fd, nullptr);
}