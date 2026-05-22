#ifndef HTTPPARSER_H
#define HTTPPARSER_H

#include <string>
#include <unordered_map>

class HttpParser {
public:
    // HTTP请求方法枚举
    enum Method {
        Invalid, GET, POST, HEAD, PUT, DELETE
    };

    // HTTP解析状态枚举
    enum ParseState {
        RequestLine, Headers, Body, Done
    };

    HttpParser() : method_(Invalid), state_(RequestLine) {}

    // 核心：解析HTTP请求报文
    bool parse(const std::string& buffer);

    // 解析结果获取接口
    Method method() const { return method_; }
    std::string path() const { return path_; }
    std::string version() const { return version_; }
    std::string getHeader(const std::string& key) const;
    std::string body() const { return body_; }
    bool isDone() const { return state_ == Done; }

    // 重置解析器，复用
    void reset();

private:
    // 解析请求行（第一行：GET /index.html HTTP/1.1）
    bool parseRequestLine(const std::string& line);
    // 解析请求头
    bool parseHeaderLine(const std::string& line);

    Method method_;
    std::string path_;
    std::string version_;
    std::unordered_map<std::string, std::string> headers_;
    std::string body_;
    ParseState state_;
};

#endif // HTTPPARSER_H
