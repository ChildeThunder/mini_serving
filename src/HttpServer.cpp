#include "HttpServer.h"
#include "HttpProtocol.h"
#include "InferenceHandler.h"
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
#include <sys/time.h>
#include <limits>
#include <errno.h>
using namespace std;
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

HttpServer::HttpServer(uint16_t port) : port_(port), pool_(4)
{
}
HttpServer::~HttpServer()
{
    if (listen_fd_ != -1)
    {
        close(listen_fd_);
    }
}
bool HttpServer::init_socket()
{
    // 1. 创建 IPv4 TCP Socket
    this->listen_fd_ = socket(AF_INET, SOCK_STREAM, 0);

    if (this->listen_fd_ == -1)
    {
        cerr << strerror(errno) << endl;
        return false;
    }

    // 2. 允许重新绑定本地地址
    int opt = 1;
    if (setsockopt(this->listen_fd_, SOL_SOCKET,
                   SO_REUSEADDR, &opt, sizeof(opt)) == -1)
    {
        cerr << strerror(errno) << endl;
        close(this->listen_fd_);
        listen_fd_ = -1;
        return false;
    }

    // 3. 设置服务器地址
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(this->port_);

    if (inet_pton(AF_INET, "127.0.0.1",
                  &addr.sin_addr) != 1)
    {
        cerr << "Invalid IP address" << endl;
        close(this->listen_fd_);
        listen_fd_ = -1;

        return false;
    }

    // 4. 绑定 IP 和端口
    if (bind(this->listen_fd_,
             reinterpret_cast<sockaddr *>(&addr),
             sizeof(addr)) == -1)
    {
        cerr << strerror(errno) << endl;
        close(this->listen_fd_);
        listen_fd_ = -1;

        return false;
    }

    // 5. 进入监听状态
    if (listen(this->listen_fd_, 128) == -1)
    {
        cerr << strerror(errno) << endl;
        close(this->listen_fd_);
        listen_fd_ = -1;

        return false;
    }
    cout << "Listening on 127.0.0.1:" << this->port_ << endl;
    return true;
}
void HttpServer::handle_client(int client_fd)
{
    timeval tv{};
    tv.tv_sec = 3;
    tv.tv_usec = 0;
    if (setsockopt(
            client_fd,
            SOL_SOCKET,
            SO_RCVTIMEO,
            &tv,
            sizeof(tv)) == -1)
    {
        cerr << "设置接收超时失败："
             << strerror(errno) << endl;
        return;
    }
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
        else if (n == -1 && errno == EAGAIN)

        {
            cerr << "3s内没有收到消息，释放连接" << endl;
            break;
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
        return;
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
            if (path == "/infer")
            {
                send_error(client_fd, "405 Method Not Allowed", "请求路径不支持", "POST");
            }
            else
            {
                send_error(client_fd, "404 Not Found", "请求路径不存在");
            }

            return;
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
            if (path == "/health")
            {
                send_error(client_fd, "405 Method Not Allowed", "请求路径不支持", "GET");
            }
            else
            {
                send_error(client_fd, "404 Method Not Allowed", "请求路径不存在");
            }
            return;
        }
        size_t body_len = 0;
        if (parse_content_length(s, body_len) == ContentLengthStatus::invalid)
        {
            cerr << "无法解析请求体长度" << endl;
            send_error(client_fd, "400 Bad Request", "Content-Length格式非法");
            return;
        };
        if (parse_content_length(s, body_len) == ContentLengthStatus::missing)
        {
            cerr << "缺少请求体长度" << endl;
            send_error(client_fd, "411 Length Required", "缺少Content-Length");
            return;
        };
        // 这里已经获取到了请求体的长度，接下来判断body是否接收完毕
        if (body_len > MAX_BODY_SIZE)
        {
            cerr << "请求体长度过大" << endl;
            send_error(client_fd, "413 Content Too Large", "请求体超过4KiB");

            return;
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
                return;
            }
        }
        if (body.size() == body_len)
        {

            vector<int32_t> datas;
            if (!parse_input(body, datas))
            {

                send_error(client_fd, "400 Bad Request", "数字格式错误");
                return;
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
            return;
        }
    }
    else
    {
        cerr << "请求类型错误" << endl;

        send_error(client_fd, "404 Method Not Allowed", "请求路径不存在");

        return;
    }
}
int HttpServer::run()
{
    if (!init_socket())
    {
        return 1;
    }
    while (1)
    {
        int client_fd = accept(this->listen_fd_, nullptr, nullptr);
        if (client_fd == -1)
        {
            int err = errno;
            if (err == EINTR)
            {
                continue;
            }
            cerr << "accept failed: "
                 << strerror(errno) << endl;
            return 1;
        }
        cout << "Client connected!" << endl;
        try
        {
            this->pool_.submit([this, client_fd]()
                               {
                               try
                               {
                                   this->handle_client(client_fd);
                                   
                               }
                               catch (const std::exception &e)
                               {
                                   std::cerr << "handle_client error:"<<e.what() << '\n';
                               }
                               close(client_fd); });
        }
        catch (const std::exception &e)
        {
            std::cerr << "submit error:" << e.what() << '\n';
            close(client_fd);
            return 1;
        }
    }
    close(this->listen_fd_);
    return 0;
}