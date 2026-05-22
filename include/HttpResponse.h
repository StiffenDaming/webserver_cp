#ifndef HTTPRESPONSE_H
#define HTTPRESPONSE_H

#include <string>
#include <unordered_map>

class HttpResponse {
public:
    // HTTP状态码
    enum StatusCode {
        Unknown = 0,
        OK = 200,
        NotFound = 404,
        BadRequest = 400,
        InternalServerError = 500
    };

    explicit HttpResponse(bool close = true) : statusCode_(Unknown), closeConnection_(close) {}

    // 设置响应信息
    void setStatusCode(StatusCode code) { statusCode_ = code; }
    void setStatusMessage(const std::string& msg) { statusMessage_ = msg; }
    void setContentType(const std::string& type) { addHeader("Content-Type", type); }
    void setBody(const std::string& body) { body_ = body; }
    void setCloseConnection(bool close) { closeConnection_ = close; }
    void addHeader(const std::string& key, const std::string& value);

    // 核心：把响应拼接成完整的HTTP报文
    std::string toString() const;

    bool closeConnection() const { return closeConnection_; }

private:
    StatusCode statusCode_;
    std::string statusMessage_;
    std::unordered_map<std::string, std::string> headers_;
    std::string body_;
    bool closeConnection_;
};

#endif // HTTPRESPONSE_H
