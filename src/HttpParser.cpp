#include "HttpParser.h"
#include <sstream>
#include <algorithm>

// 核心解析入口
bool HttpParser::parse(const std::string& buffer) {
    std::istringstream stream(buffer);
    std::string line;

    while (std::getline(stream, line)) {
        // 去掉行尾的\r
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }

        // 空行：请求头结束，进入body解析
        if (line.empty()) {
            if (state_ == Headers) {
                state_ = Body;
                // 简单处理：GET请求无body，直接完成解析
                if (method_ == GET) {
                    state_ = Done;
                    return true;
                }
            }
            continue;
        }

        // 按状态解析
        switch (state_) {
            case RequestLine:
                if (!parseRequestLine(line)) return false;
                state_ = Headers;
                break;
            case Headers:
                if (!parseHeaderLine(line)) return false;
                break;
            case Body:
                body_ += line + "\n";
                break;
            case Done:
                return true;
        }
    }

    // GET请求解析完成
    if (state_ == Headers && method_ == GET) {
        state_ = Done;
        return true;
    }

    return state_ == Done;
}

// 解析请求行
bool HttpParser::parseRequestLine(const std::string& line) {
    std::istringstream lineStream(line);
    std::string methodStr;
    lineStream >> methodStr >> path_ >> version_;

    // 解析请求方法
    if (methodStr == "GET") method_ = GET;
    else if (methodStr == "POST") method_ = POST;
    else if (methodStr == "HEAD") method_ = HEAD;
    else if (methodStr == "PUT") method_ = PUT;
    else if (methodStr == "DELETE") method_ = DELETE;
    else method_ = Invalid;

    // 处理根路径，默认指向index.html
    if (path_ == "/") {
        path_ = "/index.html";
    }

    return method_ != Invalid && !version_.empty();
}

// 解析请求头
bool HttpParser::parseHeaderLine(const std::string& line) {
    size_t colonPos = line.find(':');
    if (colonPos == std::string::npos) return false;

    std::string key = line.substr(0, colonPos);
    std::string value = line.substr(colonPos + 1);
    
    // 去掉value前后的空格
    value.erase(value.begin(), std::find_if(value.begin(), value.end(), [](int ch) {
        return !std::isspace(ch);
    }));
    value.erase(std::find_if(value.rbegin(), value.rend(), [](int ch) {
        return !std::isspace(ch);
    }).base(), value.end());

    headers_[key] = value;
    return true;
}

// 获取请求头
std::string HttpParser::getHeader(const std::string& key) const {
    auto it = headers_.find(key);
    return it != headers_.end() ? it->second : "";
}

// 重置解析器
void HttpParser::reset() {
    method_ = Invalid;
    path_.clear();
    version_.clear();
    headers_.clear();
    body_.clear();
    state_ = RequestLine;
}
