#ifndef TIMESTAMP_H
#define TIMESTAMP_H

#include <string>

// 时间戳类：用于获取当前时间，计算时间差
// 这是整个项目中最基础的工具类，不依赖任何其他模块
class TimeStamp {
public:
    TimeStamp();
    // 有参构造函数，用微秒数构造时间戳，主要作用就是给私有变量microSecondsSinceEpoch_赋值
    explicit TimeStamp(int64_t microSecondsSinceEpoch);  //explicit：禁止隐式转换
    ~TimeStamp() = default;

    // 核心静态函数：获取当前时间，计算1970年到现在的总的微秒数然后调用有参构造函数返回一个TimeStamp对象
    static TimeStamp now();

    // 转换为可读的字符串格式 "YYYYMMDD HH:MM:SS.ffffff"
    std::string toString() const;

    // 获取内部的微秒数
    int64_t microSecondsSinceEpoch() const { return microSecondsSinceEpoch_; }

    // 静态常量：一秒钟有多少微秒，1s=1000ms=1000*1000us
    static const int kMicroSecondsPerSecond = 1000 * 1000;

    // 在 TimeStamp 类的 public 区域添加
    bool operator>(const TimeStamp& rhs) const {
        return microSecondsSinceEpoch_ > rhs.microSecondsSinceEpoch_;
    }
    bool operator<(const TimeStamp& rhs) const {
        return microSecondsSinceEpoch_ < rhs.microSecondsSinceEpoch_;
    }
    bool operator==(const TimeStamp& rhs) const {
        return microSecondsSinceEpoch_ == rhs.microSecondsSinceEpoch_;
}
// 其他比较运算符可以类似添加，或者只添加你需要的。
private:
    int64_t microSecondsSinceEpoch_; // 从1970年到现在的微秒数
};

#endif // TIMESTAMP_H