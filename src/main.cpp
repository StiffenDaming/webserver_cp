#include "EventLoop.h"
#include "TcpConnection.h"
#include "HttpParser.h"
#include "HttpResponse.h"
#include "ThreadPool.h"
#include <iostream>
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <fstream>
#include <sstream>
#include <unordered_map>

// 服务器配置
const int kPort = 8888;
const std::string kWebRoot = "/home/emperor/webserver_cp/www";
const int kThreadNum = 4; // 线程池大小

// 全局变量 global variable
EventLoop* g_loop;
ThreadPool* g_threadPool;
std::unordered_map<int, TcpConnectionPtr> g_connections;

// 工具函数：设置非阻塞
int setNonBlocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) {
        perror("fcntl F_GETFL failed");
        return -1;
    }
    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1) {
        perror("fcntl F_SETFL failed");
        return -1;
    }
    return 0;
}

// 读取静态文件
std::string readFile(const std::string& filePath) {
    std::ifstream file(filePath, std::ios::in | std::ios::binary);
    if (!file.is_open()) {
        return "";
    }
    std::ostringstream oss;
    oss << file.rdbuf();
    return oss.str();
}

// 消息回调：处理HTTP请求
void onMessage(const TcpConnectionPtr& conn, const std::string& message, TimeStamp receiveTime) {
    std::cout << "[" << receiveTime.toString() << "] 收到客户端" << conn->fd() << "的HTTP请求" << std::endl;

    // 解析HTTP请求
    HttpParser parser;
    if (!parser.parse(message)) {
        // 解析失败，返回400
        HttpResponse response(true);
        response.setStatusCode(HttpResponse::BadRequest);
        response.setStatusMessage("Bad Request");
        response.setContentType("text/plain");
        response.setBody("400 Bad Request");
        conn->send(response.toString());
        conn->shutdown();
        return;
    }

    // 构造HTTP响应
    HttpResponse response(true);
    std::string filePath = kWebRoot + parser.path();
    std::string fileContent = readFile(filePath);

    if (!fileContent.empty()) {
        // 文件存在，返回200
        response.setStatusCode(HttpResponse::OK);
        response.setStatusMessage("OK");
        response.setContentType("text/html;charset=utf-8");
        response.setBody(fileContent);
        std::cout << "请求路径：" << parser.path() << "，返回200 OK" << std::endl;
    } else {
        // 文件不存在，返回404
        response.setStatusCode(HttpResponse::NotFound);
        response.setStatusMessage("Not Found");
        response.setContentType("text/html;charset=utf-8");
        response.setBody("<h1>404 Not Found</h1><p>页面不存在</p>");
        std::cout << "请求路径：" << parser.path() << "，返回404 Not Found" << std::endl;
    }

    // 发送响应
    conn->send(response.toString());
    conn->shutdown();
}

// 连接回调
void onConnection(const TcpConnectionPtr& conn) {
    if (conn->connected()) {
        std::cout << "新客户端连接，fd=" << conn->fd() << "，ip=" << conn->name() << std::endl;
        g_connections[conn->fd()] = conn;
    } else {
        std::cout << "客户端断开连接，fd=" << conn->fd() << std::endl;
        g_connections.erase(conn->fd());
    }
}

// 监听fd读事件回调：接受新连接
void onNewConnection(int listenFd, TimeStamp receiveTime) {
    struct sockaddr_in clientAddr;
    socklen_t clientAddrLen = sizeof(clientAddr);
    
    int clientFd = accept(listenFd, (struct sockaddr*)&clientAddr, &clientAddrLen);
    if (clientFd < 0) {
        perror("accept failed");
        return;
    }

    std::string clientIp = inet_ntoa(clientAddr.sin_addr);
    setNonBlocking(clientFd);

    // 创建TcpConnection
    TcpConnectionPtr conn = std::make_shared<TcpConnection>(g_loop, clientFd, clientIp, g_threadPool);
    conn->setConnectionCallback(onConnection);
    conn->setMessageCallback(onMessage);
    conn->setCloseCallback(onConnection);

    conn->connectEstablished();
}

int main() {
    std::cout << "=== 基于Reactor的高性能HTTP Web服务器启动 ===" << std::endl;

    // 1. 创建线程池
    ThreadPool threadPool(kThreadNum);
    g_threadPool = &threadPool;

    // 2. 创建EventLoop
    EventLoop loop;
    g_loop = &loop;

    // 3. 创建监听socket
    int listenFd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
    if (listenFd < 0) {
        perror("socket create failed");
        return -1;
    }

    // 4. 设置端口复用
    int opt = 1;
    setsockopt(listenFd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    // 5. 绑定地址和端口
    struct sockaddr_in serverAddr;
    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    serverAddr.sin_port = htons(kPort);

    if (bind(listenFd, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
        perror("bind failed");
        close(listenFd);
        return -1;
    }

    // 6. 开始监听
    if (listen(listenFd, 1024) < 0) {
        perror("listen failed");
        close(listenFd);
        return -1;
    }

    std::cout << "✅ 服务器启动成功！监听端口：" << kPort << std::endl;
    std::cout << "🌐 请在浏览器访问：http://localhost:" << kPort << std::endl;

    // 7. 给监听fd创建Channel
    Channel listenChannel(&loop, listenFd);
    listenChannel.setReadCallback(std::bind(onNewConnection, listenFd, std::placeholders::_1));
    listenChannel.enableReading();

    // 8. 启动事件循环
    loop.loop();

    // 9. 清理
    close(listenFd);
    threadPool.stop();

    return 0;
}