#include "Channel.h"
#include "EventLoop.h" // 注意：这里可以 include，因为 .cpp 文件不会被其他文件 include
#include <sys/epoll.h>
#include <iostream>

// 静态常量定义：事件标志位
// 这些是 Linux epoll 的标准事件定义
const int Channel::kNoneEvent = 0;
// EPOLLIN | EPOLLPRI 是按位或（|）运算，结果是 0x001 | 0x002 = 0x003（二进制 0011）。
const int Channel::kReadEvent = EPOLLIN | EPOLLPRI; // EPOLLIN: 普通数据可读；EPOLLPRI: 紧急数据可读
const int Channel::kWriteEvent = EPOLLOUT;            // EPOLLOUT: 可写

// 构造函数
Channel::Channel(EventLoop* loop, int fd)
    : loop_(loop),
      fd_(fd),
      events_(0),
      revents_(0)
{
}

// 【核心】处理事件
// 当 epoll_wait 返回后，EventLoop 会调用这个函数，根据发生的事件 (revents_) 调用对应的回调
void Channel::handleEvent(TimeStamp receiveTime) {
    // 如果发生了错误事件
    if (revents_ & EPOLLERR) {
        if (errorCallback_) errorCallback_();
    }
    
    // 如果发生了挂起事件（比如对端关闭连接）
    if (revents_ & EPOLLHUP && !(revents_ & EPOLLIN)) {
        if (closeCallback_) closeCallback_();
    }
    
    // 如果发生了读事件（包括普通可读、紧急数据可读、对端关闭连接）
    if (revents_ & (EPOLLIN | EPOLLPRI | EPOLLRDHUP)) {
        // if (readCallback_) 的意思是：如果该回调函数已被设置（为非空），而不是判断它的返回值。
        // std::function 定义了一个显式的布尔转换运算符（explicit operator bool()）。
        // 当 std::function 对象包装了一个有效的可调用对象时，转换为 true；否则为 false。
        if (readCallback_) 
        {
            // readCallback_(receiveTime); 做的事？
            // 这是调用 std::function 的 operator()，它会去执行之前存储的那个可调用对象（即 lambda）。
            // 编译器在编译时已经知道 readCallback_ 是一个 std::function 对象，它有一个 operator()。调用 readCallback_(receiveTime) 就是调用这个 operator()，不需要“去找 setReadCallback”。
            // std::function 内部通过类型擦持技术（虚函数表或函数指针）来记住你存进去的是哪个 lambda，并在调用时执行它。
            
            // 调用 readCallback_(receiveTime) 时，是否只需要保证参数对应？
            // 是的，只要参数类型匹配 std::function<void(TimeStamp)> 的签名（即接受一个 TimeStamp 参数），调用就是合法的。
            // readCallback_ 内部存储的 lambda 签名必须是 void(TimeStamp)，lambda 体内可以随意使用捕获的 readFd，但调用者（handleEvent）只负责传递 TimeStamp 参数，不需要知道 readFd。

            // 调用读事件回调，并传入事件发生的时间戳，让回调函数可以根据时间戳处理业务
            readCallback_(receiveTime);
            //std::function 本身不需要知道 TimeStamp 有哪些成员，它只关心参数类型匹配。
            //真正的成员访问发生在回调函数的定义处。例如，当你在 main 或 Server 类中设置回调时：
            //换句话说，回调的具体实现（如 lambda）会使用这个 TimeStamp 参数。
        }
    }
    
    // 如果发生了写事件
    if (revents_ & EPOLLOUT) {
        if (writeCallback_) writeCallback_();
    }
}

// ---------------- 事件管理接口实现 ----------------
// epoll事件常用运算符就3个：`|=`（加）、`&=~`（删）、`=`（清零）

// 开启读事件监听
void Channel::enableReading() {
    events_ |= kReadEvent; // 按位或：同时监听普通可读和紧急数据可读事件。kReadEvent= EPOLLIN | EPOLLPRI;
    // 本质：events_ =（ 0 | 0x003） = 0x003
    // 注意，是用二进制去进行或运算
    update();               // 通知 EventLoop 更新 epoll
}

// 关闭读事件监听
void Channel::disableReading() {
    events_ &= ~kReadEvent; // 按位与非：关闭普通可读和紧急数据可读事件监听，但保留其他事件。~kReadEvent = ~0x003 = 0xFFC（二进制 1111 1100）
    // 本质：events_ =（ 0x003 & ~0x003） = 0x000
    // 用二进制进行与非运算，0x0011 & ~0x0011 = 0x001 & 0x1100 = 0x0000
    update();
}

// 开启写事件监听
void Channel::enableWriting() {
    events_ |= kWriteEvent;
    update();
}

// 关闭写事件监听
void Channel::disableWriting() {
    events_ &= ~kWriteEvent;
    update();
}

// 关闭所有事件监听
void Channel::disableAll() {
    events_ = kNoneEvent;
    // 为什么不能用 |= 实现清零？
    // |= 只能添加位，不能移除位。
    // 例如：events_ = 0x07（二进制 0111，表示关注读+写），执行 events_ |= 0 后结果仍是 0x07。
    // 如果想清除所有位，唯一的方法是直接赋值 0（即 kNoneEvent）
    update();
}

// ---------------- 状态查询接口实现 ----------------

bool Channel::isReading() const {
    return events_ & kReadEvent;
}

bool Channel::isWriting() const {
    return events_ & kWriteEvent;
}

bool Channel::isNoneEvent() const {
    return events_ == kNoneEvent;
}

// 【核心】更新事件
// 这是 Channel 和 EventLoop 交互的桥梁
// Channel 不直接操作 epoll，而是告诉 EventLoop：“我的事件变了，你去帮我更新 epoll”
void Channel::update() {
    // this指针本身就是channel类的指针,所以实参其实是channel*channel
    loop_->updateChannel(this);
}

void Channel::remove() {
    loop_->removeChannel(this);
}