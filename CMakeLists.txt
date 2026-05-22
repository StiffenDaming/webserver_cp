cmake_minimum_required(VERSION 3.10)
project(webserver)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_BUILD_TYPE Debug)
include_directories(${PROJECT_SOURCE_DIR}/include)

# 时间戳测试
add_executable(timestamp_test src/main_timestamp.cpp src/TimeStamp.cpp)

# Reactor 测试（现在包含了 TimerManager.cpp）
add_executable(reactor_test
    src/main_reactor.cpp
    src/TimeStamp.cpp 
    src/Channel.cpp 
    src/Epoll.cpp
    src/EventLoop.cpp
    src/TimerManager.cpp
)

# 完整的 Web 服务器（原有配置不变）
add_executable(webserver_test 
    src/main.cpp 
    src/TimeStamp.cpp 
    src/Channel.cpp 
    src/Epoll.cpp 
    src/EventLoop.cpp
    src/TimerManager.cpp
    src/ThreadPool.cpp
    src/TcpConnection.cpp
    src/HttpParser.cpp
    src/HttpResponse.cpp
)

# 为两个目标链接线程库
target_link_libraries(reactor_test pthread)
target_link_libraries(webserver_test pthread)