#ifndef CHANNEL_H
#define CHANNEL_H

#include "TimeStamp.h"
#include <functional>

// 【关键】前向声明！告诉编译器 EventLoop 是一个类
// 这样就可以在不 include EventLoop.h 的情况下，使用 EventLoop* 指针
// 避免了 Channel 和 EventLoop 之间的循环引用问题
class EventLoop;

// Channel 类：Reactor 模式的核心数据结构
// 负责封装一个文件描述符(fd)，以及它感兴趣的事件和对应的回调函数
class Channel {
public:
    // 回调函数类型定义：
    // 语法：using 新名字 = 已有类型; 类似于 typedef，但更现代化
    // std::function 是定义在 <functional> 头文件中的类模板，它可以包装任何可调用对象（函数指针、lambda、成员函数等）。
    // std::function<void(TimeStamp)> 表示：返回类型为 void，接受一个 TimeStamp 类型参数的函数签名。
    using ReadCallback = std::function<void(TimeStamp)>;
    using EventCallback = std::function<void()>;

    // 构造函数
    // loop: 这个 Channel 属于哪个 EventLoop（事件循环）
    // fd: 这个 Channel 封装的文件描述符
    // 历史上，fd 就是用整数表示的（0、1、2 分别代表标准输入、输出、错误）
    Channel(EventLoop* loop, int fd);
    
    ~Channel() = default;

    // ---------------- 核心接口 ----------------

    // 处理事件：当 epoll 监听到事件发生时，调用这个函数
    // receiveTime: 事件发生的时间戳
    void handleEvent(TimeStamp receiveTime);

    // ---------------- 设置回调函数 ----------------

    // 设置读事件回调（当 fd 可读时调用）
    // setReadCallback 的参数是一个 可调用对象（函数、lambda、函数对象等），这个可调用对象的签名必须符合 ReadCallback 的要求。
    // 因此，setReadCallback 只是接收这个可调用对象，并用 std::move 移动到成员变量中，不关心参数本身。
    void setReadCallback(ReadCallback cb) { readCallback_ = std::move(cb); }
    // 设置写事件回调（当 fd 可写时调用）
    void setWriteCallback(EventCallback cb) { writeCallback_ = std::move(cb); }
    // 设置关闭事件回调
    void setCloseCallback(EventCallback cb) { closeCallback_ = std::move(cb); }
    // 设置错误事件回调
    void setErrorCallback(EventCallback cb) { errorCallback_ = std::move(cb); }

    // ---------------- 事件管理 ----------------

    // 开启读事件监听：告诉 epoll，我这个 fd 对读事件感兴趣
    void enableReading();
    // 关闭读事件监听
    void disableReading();
    // 开启写事件监听
    void enableWriting();
    // 关闭写事件监听
    void disableWriting();
    // 关闭所有事件监听
    void disableAll();

    // ---------------- 状态查询 ----------------

    // 是否正在监听读事件
    bool isReading() const;
    // 是否正在监听写事件
    bool isWriting() const;
    // 是否没有监听任何事件
    bool isNoneEvent() const;

    // ---------------- Getter ----------------

    // 获取封装的文件描述符
    int fd() const { return fd_; }
    // 获取当前关注的事件（epoll 要用）
    int events() const { return events_; }
    // 设置当前发生的事件（由 epoll 设置）
    void set_revents(int revt) { revents_ = revt; }
    // 获取所属的 EventLoop
    EventLoop* ownerLoop() { return loop_; }

    // 从 epoll 中移除当前 Channel（通常用于连接关闭时）
    void remove();

public:
    // 事件标志位（静态常量，类内声明）
    static const int kNoneEvent;
    static const int kReadEvent;
    static const int kWriteEvent;

private:
    // 【核心】更新事件：当关注的事件改变时，通知 EventLoop 去更新 epoll
    // 这是 Channel 和 EventLoop 交互的核心接口
    void update();

    // ---------------- 成员变量 ----------------

    EventLoop* loop_;       // 所属的事件循环
    const int fd_;          // 封装的文件描述符
    int events_;            // 当前关注的事件（我们想让 epoll 监听什么）
    // Epoll中获得
    int revents_;           // 当前实际发生的事件（epoll 告诉我们发生了什么）

    // 回调函数
    // ReadCallback它代表 std::function<void(TimeStamp)> 这个具体类型。
    // readCallback_ 是该类型的一个对象（更准确地说，是一个函数对象，即 std::function 的实例）。
    // 它不是函数，也不是类，而是一个可以 像函数一样调用的对象 （因为它重载了 operator()）。
        // 为什么 Channel.h 中只有声明，没有定义？
    // Channel.h 中只声明了成员变量 ReadCallback readCallback_;。
    // 这只是一个声明，告诉编译器「有一个名字叫 readCallback_ 的东西，它的类型是 ReadCallback」。
    // 真正的定义（即 lambda 的具体实现）是在 main.cpp 中通过 setReadCallback 传入的。
    // 这与「函数声明在 .h，定义在 .cpp」类似，只是这里的定义是运行时动态绑定的。
        // 核心关系链
    // ReadCallback 只是 std::function<void(TimeStamp)> 的一个别名，没有新增任何功能。
    // readCallback_ 是一个具体的 std::function 对象，它可以存储任何签名匹配 void(TimeStamp) 的可调用对象。
        // readCallback_ 只能存一个可调用对象吗？
    // 是的，std::function 对象在同一时刻只能存储一个可调用对象（lambda、函数指针、函数对象等）。
    // 当你第二次调用 setReadCallback 时，新赋值的对象会替换旧的（通过移动赋值或拷贝赋值）。
    // 它不像 vector 可以存多个回调，如果需要多个回调，可以用 vector<std::function<...>>。
    ReadCallback readCallback_;   // 读事件回调
    EventCallback writeCallback_; // 写事件回调
    EventCallback closeCallback_; // 关闭事件回调
    EventCallback errorCallback_; // 错误事件回调

    // 事件标志位（这些宏定义在 <sys/epoll.h> 里）
    // 为了不依赖 epoll.h，我们可以自己定义，或者直接用数字
    // 这里为了简化，我们直接在 .cpp 里使用 EPOLLIN/EPOLLOUT
};

#endif // CHANNEL_H