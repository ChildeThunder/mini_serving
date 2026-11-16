#include "HttpProtocol.h"
#include "HttpServer.h"
#include <limits>
ContentLengthStatus parse_content_length(
    const std::string &header,
    size_t &length)
{
    size_t pos = header.find("Content-Length:");
    if (pos == string::npos)
    {
        return ContentLengthStatus::missing;
    }
    pos += 15;
    size_t end_pos = header.find("\r\n", pos);
    if (end_pos == string::npos)
    {
        return ContentLengthStatus::invalid;
    }
    string s_len = header.substr(pos, end_pos - pos);
    // 去除两端的空格和制表符
    size_t first = s_len.find_first_not_of(" \t");
    if (first == string::npos)
        return ContentLengthStatus::invalid;

    size_t last = s_len.find_last_not_of(" \t");
    s_len = s_len.substr(first, last - first + 1);

    // 必须全部是数字
    if (s_len.find_first_not_of("0123456789") != string::npos)
        return ContentLengthStatus::invalid;

    try
    {
        unsigned long long value = stoull(s_len);

        if (value > numeric_limits<size_t>::max())
            return ContentLengthStatus::invalid;

        length = static_cast<size_t>(value);
    }
    catch (const exception &e)
    {
        return ContentLengthStatus::invalid;
    }

    return ContentLengthStatus::ok;
}
bool send_error(
    int client_fd,
    const string &status,
    const string &message,
    const string &allow)
{
    string bad_body = "{\"error\":\"" + message + "\"}";
    string bad_response =
        "HTTP/1.1 " + status + "\r\n"
                               "Content-Type: application/json\r\n"
                               "Content-Length: " +
        to_string(bad_body.size()) + "\r\n";
    // 405 错误需要 Allow 响应头
    if (!allow.empty())
    {
        bad_response += "Allow: " + allow + "\r\n";
    }

    bad_response +=
        "Connection: close\r\n"
        "\r\n" +
        bad_body;
    return send_all(client_fd, bad_response.data(), bad_response.size());
}
