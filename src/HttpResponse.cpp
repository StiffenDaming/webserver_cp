#include "HttpResponse.h"
#include <sstream>

// 添加响应头
void HttpResponse::addHeader(const std::string& key, const std::string& value) {
    headers_[key] = value;
}

// 拼接完整HTTP响应报文
std::string HttpResponse::toString() const {
    std::ostringstream oss;

    // 1. 响应行
    oss << "HTTP/1.1 " << statusCode_ << " " << statusMessage_ << "\r\n";

    // 2. 响应头
    for (const auto& header : headers_) {
        oss << header.first << ": " << header.second << "\r\n";
    }

    // 3. 连接状态
    if (closeConnection_) {
        oss << "Connection: close\r\n";
    } else {
        oss << "Connection: Keep-Alive\r\n";
    }

    // 4. 内容长度
    oss << "Content-Length: " << body_.size() << "\r\n";

    // 5. 空行分隔头和body
    oss << "\r\n";

    // 6. 响应体
    oss << body_;

    return oss.str();
}
