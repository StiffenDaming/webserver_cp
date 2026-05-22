// 包含所有核心模块头文件
#include "EventLoop.h"
#include "Channel.h"
#include "TimeStamp.h"
// 管道相关头文件（模拟IO事件）
#include <unistd.h>
#include <iostream>
// 用于管道写入
#include <string.h>

// ---------------- 测试回调函数 ----------------
// 当管道读端有数据可读时，自动调用这个函数
void readCallback(int readFd, TimeStamp receiveTime) {
    std::cout << "\n=====================================" << std::endl;
    std::cout << "✅ 【Channel 回调触发】" << std::endl;
    std::cout << "⏰ 事件发生时间：" << receiveTime.toString() << std::endl;
    std::cout << "📂 触发事件的文件描述符：" << readFd << std::endl;
    std::cout << "📝 事件类型：管道读事件（模拟HTTP请求）" << std::endl;
    
    // 读取管道里的测试数据
    char buf[1024] = {0};
    ssize_t n = ::read(readFd, buf, sizeof(buf));
    if (n > 0) {
        std::cout << "📥 读取到数据：" << buf << std::endl;
    }
    std::cout << "=====================================\n" << std::endl;
}

// ---------------- 主函数：Reactor 核心测试 ----------------
int main() {
    std::cout << "🎉 开始测试 Reactor 核心引擎" << std::endl;
    std::cout << "=====================================\n" << std::endl;

    // ========== 步骤1：创建管道 ==========
    int pipefd[2];
    if (::pipe(pipefd) < 0) {
        perror("pipe 创建失败");
        return 1;
    }
    int readFd = pipefd[0];  // 读端（监听这个fd）
    int writeFd = pipefd[1]; // 写端（主动写数据触发读事件）

    // ========== 步骤2：创建 EventLoop ==========
    EventLoop loop;

    // ========== 步骤3：为读端fd创建 Channel ==========
    Channel channel(&loop, readFd);

    // ========== 步骤4：设置读事件回调 ==========
    // 【核心注释】
    // 为什么需要 lambda？
    // readCallback 原本需要两个参数 (int, TimeStamp)，
    // 但 Channel 期望的回调类型是 std::function<void(TimeStamp)>（单参数）。
    // 通过 lambda 捕获 readFd，将 readFd 固定（适配），
    // 使回调接口统一为 void(TimeStamp)，同时保留了 readFd 的值。
    // 当读事件发生时，EventLoop 调用 channel.handleEvent(now)，
    // 内部执行 readCallback_(receiveTime)，这里的 receiveTime 就是 lambda 的参数 ts，
    // lambda 再调用 readCallback(readFd, ts)，完成参数传递。
    channel.setReadCallback([readFd](TimeStamp ts) {
        readCallback(readFd, ts);
    });

    // ========== 步骤5：开启读事件监听 ==========
    // 【核心注释】
    // enableReading() 内部执行 events_ |= kReadEvent，
    // 将 events_ 的读事件位设为 1（通过位或操作）。
    // 然后调用 update() -> loop_->updateChannel(this) 
    // -> epoll_->updateChannel(channel) -> epoll_ctl(EPOLL_CTL_ADD/MOD)。
    // 最终通知内核：开始监听该 fd 的读事件（包括 EPOLLIN 和 EPOLLPRI）。
    // 如果没有这一步，epoll 不会关注该 fd，即使管道有数据也不会触发回调。
    channel.enableReading();

    // ========== 步骤6：主动写管道，触发读事件 ==========
    const char* testMsg = "Hello Reactor!";
    ::write(writeFd, testMsg, strlen(testMsg));
    std::cout << "📤 已向管道写入测试数据：" << testMsg << std::endl;
    std::cout << "⏳ 等待 Epoll 监听事件...\n" << std::endl;

    // ========== 步骤7：启动事件循环 ==========
    // loop() 内部 while (!quit_) 循环调用 epoll_->wait()，
    // 获取就绪的 Channel 列表，并依次调用 channel->handleEvent(TimeStamp::now())。
    loop.loop();

    // 清理
    ::close(readFd);
    ::close(writeFd);

    return 0;
}