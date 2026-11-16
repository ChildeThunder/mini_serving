
#include <sys/socket.h>
#include <unistd.h>
#include <cerrno>
#include <cstring>
#include <iostream>
#include <string>
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
            0);
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
    if (total_sent == len)
        return true;
    return false;
}
int main()
{
    int fds[2];

    // 1. 创建一对已连接的 Socket
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, fds) == -1)
    {
        cerr << "socketpair failed: "
             << strerror(errno) << endl;
        return 1;
    }

    // 2. A 端发送数据
    const char message[] = "Hello Socket!";

    ssize_t sent = send(
        fds[0],
        message,
        sizeof(message) - 1,
        0);

    if (sent == -1)
    {
        cerr << "send failed: "
             << strerror(errno) << endl;
        close(fds[0]);
        close(fds[1]);
        return 1;
    }

    cout << "send 返回: " << sent << endl;

    // 3. B 端接收数据
    char buffer[1024]{};

    ssize_t received = recv(
        fds[1],
        buffer,
        sizeof(buffer),
        0);

    if (received > 0)
    {
        cout << "recv 返回: " << received << endl;

        cout << "接收到: ";
        cout.write(buffer, received);
        cout << endl;
    }
    else if (received == 0)
    {
        cout << "对端关闭了发送方向" << endl;
        close(fds[0]);
        close(fds[1]);
        return 1;
    }
    else
    {
        cerr << "recv failed: "
             << strerror(errno) << endl;
        //  释放两个 Socket
        close(fds[0]);
        close(fds[1]);

        return received < 0 ? 1 : 0;
    }
    // B端发送数据
    // cout << buffer << endl;
    // cout << "现在messgae的大小是：" << strlen(message_) << endl;
    bool success = send_all(
        fds[1],
        buffer,
        static_cast<size_t>(received));
    if (success == false)
    {
        cerr << "send failed: "
             << strerror(errno) << endl;
        close(fds[0]);
        close(fds[1]);
        return 1;
    }

    cout << "send succcess" << endl;
    // A端接收数据
    char buffer_[1024]{};
    ssize_t received_ = recv(
        fds[0],
        buffer_,
        sizeof(buffer_),
        0);
    if (received_ > 0)
    {
        cout << "recv 返回: " << received_ << endl;

        cout << "接收到: ";
        cout.write(buffer_, received_);
        cout << endl;
    }
    else if (received_ == 0)
    {
        cout << "对端关闭了发送方向" << endl;
    }
    else
    {
        cerr << "recv failed: "
             << strerror(errno) << endl;
    }

    //  释放两个 Socket
    close(fds[0]);
    close(fds[1]);

    return received_ < 0 ? 1 : 0;
}
