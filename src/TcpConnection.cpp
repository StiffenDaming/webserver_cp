#include "TcpConnection.h"
#include <iostream>
#include <unistd.h>
#include <sys/socket.h>
#include <errno.h>

TcpConnection::TcpConnection(EventLoop* loop, int fd, const std::string& name, ThreadPool* threadPool)
    : loop_(loop),
      channel_(std::make_unique<Channel>(loop, fd)),
      name_(name),
      state_(kConnecting),
      threadPool_(threadPool)
{
    // 设置Channel的回调
    channel_->setReadCallback(std::bind(&TcpConnection::handleRead, this, std::placeholders::_1));
    channel_->setWriteCallback(std::bind(&TcpConnection::handleWrite, this));
    channel_->setCloseCallback(std::bind(&TcpConnection::handleClose, this));
    channel_->setErrorCallback(std::bind(&TcpConnection::handleError, this));
}

TcpConnection::~TcpConnection() {
    std::cout << "TcpConnection " << name_ << " 析构，fd=" << channel_->fd() << std::endl;
}

// 连接建立完成
void TcpConnection::connectEstablished() {
    loop_->assertInLoopThread();
    setState(kConnected);
    channel_->enableReading(); // 开启读事件监听

    if (connectionCallback_) {
        connectionCallback_(shared_from_this());
    }
}

// 连接关闭
void TcpConnection::connectDestroyed() {
    loop_->assertInLoopThread();
    if (state_ == kConnected) {
        setState(kDisconnected);
        channel_->disableAll();

        if (connectionCallback_) {
            connectionCallback_(shared_from_this());
        }
    }
    channel_->remove();
}

// 处理读事件
void TcpConnection::handleRead(TimeStamp receiveTime) {
    loop_->assertInLoopThread();
    char buf[4096];
    ssize_t n = ::read(channel_->fd(), buf, sizeof(buf));

    if (n > 0) {
        inputBuffer_.append(buf, n);
        // 把消息处理放到线程池执行
        if (messageCallback_) {
            threadPool_->addTask([this, receiveTime]() {
                messageCallback_(shared_from_this(), inputBuffer_, receiveTime);
            });
        }
    } else if (n == 0) {
        handleClose();
    } else {
        if (errno != EAGAIN && errno != EWOULDBLOCK) {
            perror("read error");
            handleError();
        }
    }
}

// 处理写事件
void TcpConnection::handleWrite() {
    loop_->assertInLoopThread();
    if (channel_->isWriting()) {
        ssize_t n = ::write(channel_->fd(), outputBuffer_.data(), outputBuffer_.size());
        if (n > 0) {
            outputBuffer_.erase(0, n);
            if (outputBuffer_.empty()) {
                channel_->disableWriting(); // 数据发完，关闭写事件
            }
        } else {
            perror("write error");
        }
    }
}

// 处理关闭事件
void TcpConnection::handleClose() {
    loop_->assertInLoopThread();
    setState(kDisconnected);
    channel_->disableAll();

    if (closeCallback_) {
        closeCallback_(shared_from_this());
    }
}

// 处理错误事件
void TcpConnection::handleError() {
    loop_->assertInLoopThread();
    perror("TcpConnection error");
}

// 发送数据
void TcpConnection::send(const std::string& message) {
    if (state_ == kConnected) {
        if (loop_->isInLoopThread()) {
            sendInLoop(message);
        } else {
            loop_->runInLoop(std::bind(&TcpConnection::sendInLoop, this, message));
        }
    }
}

// 在EventLoop线程中发送数据
void TcpConnection::sendInLoop(const std::string& message) {
    loop_->assertInLoopThread();
    if (state_ == kDisconnected) {
        return;
    }

    // 如果输出缓冲区为空，直接发送
    if (outputBuffer_.empty()) {
        ssize_t n = ::write(channel_->fd(), message.data(), message.size());
        if (n >= 0) {
            if (static_cast<size_t>(n) < message.size()) {
                // 没发完，剩下的放到输出缓冲区
                outputBuffer_.append(message.data() + n, message.size() - n);
                channel_->enableWriting(); // 开启写事件监听
            }
        } else {
            perror("write error");
        }
    } else {
        // 输出缓冲区不为空，直接追加
        outputBuffer_.append(message);
    }
}

// 关闭连接
void TcpConnection::shutdown() {
    if (state_ == kConnected) {
        setState(kDisconnecting);
        loop_->runInLoop(std::bind(&TcpConnection::shutdownInLoop, this));
    }
}

// 在EventLoop线程中关闭连接
void TcpConnection::shutdownInLoop() {
    loop_->assertInLoopThread();
    if (!channel_->isWriting()) {
        ::shutdown(channel_->fd(), SHUT_WR);
    }
}