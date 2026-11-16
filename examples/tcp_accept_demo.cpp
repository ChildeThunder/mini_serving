
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <sys/types.h>
#include <cerrno>
#include <cstring>
#include <iostream>

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
            break;
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
        // 创建接收缓冲区
        char buffer[1024];
        while (true)
        {
            ssize_t n = recv(
                client_fd,
                buffer,
                sizeof(buffer),
                0);
            cout << "通信接收成功：" << n << endl;
            if (n > 0)
            {
                // 收到 n 字节
                // 使用 send_all() 原样回传
                int message = send_all(client_fd, buffer, n);
                if (message <= 0)
                {
                    cerr << "通信发送失败" << endl;
                    close(client_fd);
                    close(listen_fd);
                    return 0;
                }
                cout << "通信发送成功：" << n << endl;
            }
            else if (n == 0)
            {
                // 客户端正常结束发送
                // 跳出内层循环
                cout << "对方关闭了写端" << endl;
                break;
            }
            else
            {
                // 如果 errno == EINTR，重试
                // 其他错误结束本次客户端处理
                if (errno == EINTR)
                {
                    continue;
                }
                cerr << "通信接收失败"
                     << strerror(errno) << endl;
                close(client_fd);
                break;
            }
        }
        close(client_fd);
    }
    close(listen_fd);

    return 0;
}
