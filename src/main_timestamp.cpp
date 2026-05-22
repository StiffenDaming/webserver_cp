#include "TimeStamp.h"
#include <iostream>
#include <unistd.h>

int main() {
    std::cout << "=== TimeStamp 类测试 ===" << std::endl;
    
    // 1. 获取当前时间
    TimeStamp now = TimeStamp::now();
    std::cout << "当前时间: " << now.toString() << std::endl;
    
    // 2. 测试sleep
    std::cout << "正在 sleep 1 秒..." << std::endl;
    TimeStamp start = TimeStamp::now();
    sleep(1);
    TimeStamp end = TimeStamp::now();
    
    // 3. 计算耗时，static_cast<>是C++的类型转换操作符，用于显式地转换类型，这里把微秒差转换成秒
    double diff = static_cast<double>(end.microSecondsSinceEpoch() - start.microSecondsSinceEpoch()) 
                  / TimeStamp::kMicroSecondsPerSecond;
    std::cout << "实际耗时: " << diff << " 秒" << std::endl;
    
    std::cout << "=== 测试通过！===" << std::endl;
    return 0;
}