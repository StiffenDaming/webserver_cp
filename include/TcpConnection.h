#ifndef TCPCONNECTION_H
#define TCPCONNECTION_H

#include "Channel.h"
#include "EventLoop.h"
#include "TimeStamp.h"
#include "ThreadPool.h"
#include <memory>
#include <string>
#include <functional>

class TcpConnection;
using TcpConnectionPtr = std::shared_ptr<TcpConnection>;

// 连接回调类型
using ConnectionCallback = std::function<void(const TcpConnectionPtr&)>;
// 消息回调类型
using MessageCallback = std::function<void(const TcpConnectionPtr&, const std::string&, TimeStamp)>;
// 关闭回调类型
using CloseCallback = std::function<void(const TcpConnectionPtr&)>;

class TcpConnection : public std::enable_shared_from_this<TcpConnection> {
public:
    TcpConnection(EventLoop* loop, int fd, const std::string& name, ThreadPool* threadPool);
    ~TcpConnection();

    // 连接建立完成
    void connectEstablished();
    // 连接关闭
    void connectDestroyed();

    // 发送数据
    void send(const std::string& message);
    // 关闭连接
    void shutdown();

    // 设置回调
    void setConnectionCallback(ConnectionCallback cb) { connectionCallback_ = std::move(cb); }
    void setMessageCallback(MessageCallback cb) { messageCallback_ = std::move(cb); }
    void setCloseCallback(CloseCallback cb) { closeCallback_ = std::move(cb); }

    // 获取信息
    EventLoop* getLoop() const { return loop_; }
    const std::string& name() const { return name_; }
    int fd() const { return channel_->fd(); }
    bool connected() const { return state_ == kConnected; }

private:
    // 连接状态
    enum State {
        kDisconnected,
        kConnecting,
        kConnected,
        kDisconnecting
    };

    // 处理读事件
    void handleRead(TimeStamp receiveTime);
    // 处理写事件
    void handleWrite();
    // 处理关闭事件
    void handleClose();
    // 处理错误事件
    void handleError();

    // 在EventLoop线程中发送数据
    void sendInLoop(const std::string& message);
    // 在EventLoop线程中关闭连接
    void shutdownInLoop();

    // 设置连接状态
    void setState(State s) { state_ = s; }

    EventLoop* loop_;               // 所属的EventLoop
    std::unique_ptr<Channel> channel_; // 对应的Channel
    std::string name_;              // 连接名称
    State state_;                   // 连接状态
    ThreadPool* threadPool_;        // 线程池指针
    std::string inputBuffer_;       // 输入缓冲区
    std::string outputBuffer_;      // 输出缓冲区

    ConnectionCallback connectionCallback_; // 连接回调
    MessageCallback messageCallback_;       // 消息回调
    CloseCallback closeCallback_;           // 关闭回调
};

#endif // TCPCONNECTION_H