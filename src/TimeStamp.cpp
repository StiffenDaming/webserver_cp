#include "TimeStamp.h"
#include <sys/time.h>
#include <stdio.h>
#include <time.h>

// 默认构造函数：初始化为0
TimeStamp::TimeStamp() : microSecondsSinceEpoch_(0) {}

// 带参数的构造函数：用给定的微秒数初始化
TimeStamp::TimeStamp(int64_t microSecondsSinceEpochArg)
    : microSecondsSinceEpoch_(microSecondsSinceEpochArg) {}

// 核心函数：获取当前系统时间
TimeStamp TimeStamp::now() {
    // timeval结构体包含两个成员：tv_sec（秒）和tv_usec（微秒），gettimeofday函数会填充这个结构体，获取当前时间的秒和微秒部分
    struct timeval tv;
    gettimeofday(&tv, NULL);    // gettimeofday 是Linux系统调用，获取当前时间

    int64_t seconds = tv.tv_sec;
    // 把秒和微秒拼起来，转换成总的微秒数,赋值给microSecondsSinceEpoch_
    return TimeStamp(seconds * kMicroSecondsPerSecond + tv.tv_usec);
}

// 把时间戳转换成 "20260325 18:00:00.123456" 这样的字符串
std::string TimeStamp::toString() const {
    char buf[64] = {0};
    // time_t是一个整数类型，表示从1970年1月1日00:00:00 UTC到现在的秒数
    time_t seconds = static_cast<time_t>(microSecondsSinceEpoch_ / kMicroSecondsPerSecond);
    int microseconds = static_cast<int>(microSecondsSinceEpoch_ % kMicroSecondsPerSecond);
    
    // tm结构体包含了年月日时分秒等信息，gmtime_r函数会把秒数转换成tm结构体，表示对应的年月日时分秒等信息
    struct tm tm_time;
    gmtime_r(&seconds, &tm_time);
    
    // 格式化字符串
    snprintf(buf, sizeof(buf), "%4d%02d%02d %02d:%02d:%02d.%06d",
             tm_time.tm_year + 1900, tm_time.tm_mon + 1, tm_time.tm_mday,
             tm_time.tm_hour + 8, tm_time.tm_min, tm_time.tm_sec, // +8是为了转成北京时间
             microseconds);
    return buf; //隐式转换成std::string
}