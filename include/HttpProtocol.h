// HTTP 协议层
// 负责解析请求行、Content-Length、HTTP 消息，以及构造正常或错误响应。
#pragma once
#include <iostream>
#include <string>
using namespace std;
enum class ContentLengthStatus
{
    ok,
    missing,
    invalid
};
// 从 HTTP Header 中获取请求体长度
ContentLengthStatus parse_content_length(
    const std::string &header,
    size_t &length);
// 给对端发送报错信息
bool send_error(
    int client_fd,
    const string &status,
    const string &message,
    const string &allow = "");