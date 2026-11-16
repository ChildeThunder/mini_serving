
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <sys/types.h>
#include <cerrno>
#include <cstring>
#include <iostream>
#include <vector>
#include <memory>
#include <string>
#include <sstream>
#include <limits>
using namespace std;
bool send_all(int fd, const char *data, size_t len);
bool recv_exact(int fd, char *buffer, size_t len);

// 从 HTTP Header 中获取请求体长度
bool parse_content_length(
    const std::string &header,
    size_t &length);

// 解析输入数字
bool parse_input(
    const std::string &body,
    std::vector<int32_t> &input);

// 执行模拟推理
int64_t mock_inference(
    const std::vector<int32_t> &input);

// 给对端发送报错信息
bool send_error(
    int client_fd,
    const string &status,
    const string &message,
    const string &allow = "");

int main()
{
    // 1. 创建 IPv4 TCP Socket
    int listen_fd = socket(AF_INET, SOCK_STREAM, 0);

    if (listen_fd == -1)
    {
        cerr << strerror(errno) << endl;
        return 1;
    }

    // 2. 允许重新绑定本地地址
    int opt = 1;
    if (setsockopt(listen_fd, SOL_SOCKET,
                   SO_REUSEADDR, &opt, sizeof(opt)) == -1)
    {
        cerr << strerror(errno) << endl;
        close(listen_fd);
        return 1;
    }

    // 3. 设置服务器地址
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(9090);

    if (inet_pton(AF_INET, "127.0.0.1",
                  &addr.sin_addr) != 1)
    {
        cerr << "Invalid IP address" << endl;
        close(listen_fd);
        return 1;
    }

    // 4. 绑定 IP 和端口
    if (bind(listen_fd,
             reinterpret_cast<sockaddr *>(&addr),
             sizeof(addr)) == -1)
    {
        cerr << strerror(errno) << endl;
        close(listen_fd);
        return 1;
    }

    // 5. 进入监听状态
    if (listen(listen_fd, 128) == -1)
    {
        cerr << strerror(errno) << endl;
        close(listen_fd);
        return 1;
    }

    cout << "Listening on 127.0.0.1:9090" << endl;
    // 进入循环，可以多次接收客户端的通信
    constexpr size_t MAX_HEADER_SIZE = 8 * 1024;
    constexpr size_t MAX_BODY_SIZE = 4 * 1024;
    while (true)
    {

        // 等待一个客户端连接
        int client_fd = accept(listen_fd, nullptr, nullptr);

        if (client_fd == -1)
        {
            cerr << strerror(errno) << endl;
            close(listen_fd);
            return 1;
        }
        cout << "Client connected!" << endl;
        string s;
        char buffer[1024];
        bool header_complete = false;
        // 接收消息头部
        while (true)
        {
            int n = recv(client_fd, buffer, sizeof(buffer), 0);
            if (n > 0)
            {
                cout << "接收到了数据，总长度：" << n << endl;
                s.append(buffer, n);
                size_t end = s.find("\r\n\r\n");
                if (end != string::npos)
                {
                    if (end + 4 > MAX_HEADER_SIZE)
                    {
                        cout << "消息头部过大，舍弃" << endl;
                        break;
                    }
                    header_complete = true;
                    break;
                }
                if (s.size() > MAX_HEADER_SIZE)
                {
                    cerr << "HTTP Header 超过限制" << endl;
                    break;
                }
            }
            else if (n == 0)
            {
                cerr << "客户端提前结束发送，Header 不完整"
                     << endl;
                break;
            }
            else if (errno == EINTR)
            {
                continue;
            }
            else
            {
                cerr << "recv failed: "
                     << strerror(errno) << endl;
                break;
            }
        }
        if (header_complete == false)
        {
            close(client_fd);
            continue;
        }
        size_t pos = s.find("\r\n");
        string request_line = s.substr(0, pos);
        istringstream iss(request_line);
        string method;
        string path;
        string version;
        iss >> method >> path >> version;

        if (method == "GET")
        {
            if (path != "/health")
            {
                cerr << "收到错误路径" << endl;

                send_error(client_fd, "404 Not Found", "请求路径不存在");
                close(client_fd);
                continue;
            }
            std::string body = R"({"status":"ok"})";
            std::string response =
                "HTTP/1.1 200 OK\r\n"
                "Content-Type: application/json\r\n"
                "Content-Length: " +
                std::to_string(body.size()) + "\r\n"
                                              "Connection: close\r\n"
                                              "\r\n" +
                body;
            cout << "response.size() = "
                 << response.size() << endl;

            cout << "response内容：" << endl;
            cout << response << endl;

            bool is_send = send_all(client_fd, response.data(), response.size());
            if (is_send)
            {
                cout << "response发送成功" << endl;
            }
        }
        else if (method == "POST")
        {
            if (path != "/infer")
            {
                cerr << "收到错误路径" << endl;
                send_error(client_fd, "404", "请求路径不存在");
                close(client_fd);
                continue;
            }
            size_t body_len = 0;
            if (parse_content_length(s, body_len) == false)
            {
                cerr << "无法解析请求体长度" << endl;
                send_error(client_fd, "400 Bad Request", "Content-Length格式非法");
                close(client_fd);
                continue;
            };
            // 这里已经获取到了请求体的长度，接下来判断body是否接收完毕
            if (body_len > MAX_BODY_SIZE)
            {
                cerr << "请求体长度过大" << endl;
                send_error(client_fd, "413 Content Too Large", "请求体超过4KiB");

                close(client_fd);
                continue;
            }
            size_t body_start = s.find("\r\n\r\n");
            body_start += 4;
            string body = s.substr(body_start);

            if (body.size() < body_len)
            {
                size_t already = body.size();
                body.resize(body_len);
                bool is_ok = recv_exact(client_fd, body.data() + already, body_len - already);
                if (!is_ok)
                {
                    cerr << "body接收不完整" << endl;
                    close(client_fd);
                    continue;
                }
            }
            if (body.size() == body_len)
            {

                vector<int32_t> datas;
                if (!parse_input(body, datas))
                {

                    send_error(client_fd, "400 Bad Request", "数字格式错误");
                    close(client_fd);
                    continue;
                }

                int64_t answer_request = mock_inference(datas);
                string response_body =
                    "{\"sum\":" + to_string(answer_request) + "}";
                std::string response =
                    "HTTP/1.1 200 OK\r\n"
                    "Content-Type: application/json\r\n"
                    "Content-Length: " +
                    std::to_string(response_body.size()) + "\r\n"
                                                           "Connection: close\r\n"
                                                           "\r\n" +
                    response_body;

                bool is_send = send_all(client_fd, response.data(), response.size());
                if (is_send)
                {
                    cout << "response发送成功" << endl;
                }
            }
            else if (body.size() > body_len)
            {
                cerr << "body长度过大" << endl;
                send_error(client_fd, "413 Content Too Large", "请求体超过4KiB");
                close(client_fd);
                continue;
            }
        }
        else
        {
            cerr << "请求类型错误" << endl;

            send_error(client_fd, "405 Method Not Allowed", "请求方法不支持");

            close(client_fd);
            continue;
        }
        close(client_fd);
    }

    close(listen_fd);

    return 0;
}
bool send_all(int fd, const char *data, size_t len)
{
    size_t total_sent = 0;
    while (total_sent < len)
    {
        ssize_t n = send(
            fd,
            data + total_sent,
            len - total_sent,
            MSG_NOSIGNAL);
        if (n > 0)
        {
            total_sent += n;
        }
        else if (n == -1 && errno == EINTR)
        {
            continue;
        }
        else
        {
            return false;
        }
    }
    return true;
}
bool recv_exact(int fd, char *buffer, size_t len)
{
    size_t total_received = 0;

    while (total_received < len)
    {
        ssize_t n = recv(
            fd,
            buffer + total_received,
            len - total_received,
            0);
        if (n > 0)
        {
            total_received += n;
        }
        else if (n == 0)
        {
            cout << "对方结束写端" << endl;
            return false;
        }
        else
        {
            if (errno == EINTR)
            {
                continue;
            }
            cerr << "通讯接收失败" << strerror(errno) << endl;
            return false;
        }
    }

    return true;
}
bool parse_content_length(const std::string &header,
                          size_t &length)
{
    size_t pos = header.find("Content-Length:");
    if (pos == string::npos)
    {
        return false;
    }
    pos += 15;
    size_t end_pos = header.find("\r\n", pos);
    if (end_pos == string::npos)
    {
        return false;
    }
    string s_len = header.substr(pos, end_pos - pos);
    // 去除两端的空格和制表符
    size_t first = s_len.find_first_not_of(" \t");
    if (first == string::npos)
        return false;

    size_t last = s_len.find_last_not_of(" \t");
    s_len = s_len.substr(first, last - first + 1);

    // 必须全部是数字
    if (s_len.find_first_not_of("0123456789") != string::npos)
        return false;

    try
    {
        unsigned long long value = stoull(s_len);

        if (value > numeric_limits<size_t>::max())
            return false;

        length = static_cast<size_t>(value);
    }
    catch (const exception &e)
    {
        return false;
    }

    return true;
}
bool parse_input(const std::string &body,
                 std::vector<int32_t> &input)
{
    input.clear();
    size_t start = 0;
    size_t pos;
    while (start < body.size())
    {
        // 跳过空格
        while (start < body.size() && body[start] == ' ')
        {
            start++;
        }
        if (start >= body.size())
        {
            break;
        }
        pos = body.find(' ', start);
        string num_str;
        if (pos == string::npos)
        {
            num_str = body.substr(start);
            start = body.size();
        }
        else
        {
            num_str = body.substr(start, pos - start);
            start = pos + 1;
        }
        int32_t val;
        try
        {
            val = stoi(num_str);
        }
        catch (const logic_error &e)
        {
            cerr << e.what() << endl;
            return false;
        }
        input.push_back(val);
    }
    return true;
}
int64_t mock_inference(
    const std::vector<int32_t> &input)
{
    int64_t sum = 0;
    for (auto i : input)
    {
        sum += i;
    }
    // this_thread::sleep_for(std::chrono::seconds(1));

    return sum;
}
bool send_error(int client_fd,
                const string &status,
                const string &message,
                const string &allow = "")
{
    string bad_body = "{\"error\":\"" + message + "\"}";
    string bad_response =
        "HTTP/1.1 " + status + "\r\n"
                               "Content-Type: application/json\r\n"
                               "Content-Length: " +
        to_string(bad_body.size()) + "\r\n"
                                     "Connection: close\r\n"
                                     "\r\n" +
        bad_body;
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