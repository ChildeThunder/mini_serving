
#include <sys/socket.h>
#include <sys/epoll.h>
#include <unistd.h>
#include <iostream>
#include <cstring>
#include <fcntl.h>
#include <cerrno>
using namespace std;

int main()
{
    int fds[2];

    if (socketpair(AF_UNIX, SOCK_STREAM, 0, fds) == -1)
    {
        cerr << "socketpair failed\n";
        return 1;
    }
    int flags = fcntl(fds[1], F_GETFL, 0);
    if (flags == -1 || fcntl(fds[1], F_SETFL, flags | O_NONBLOCK) == -1)
    {
        cerr << "设置非阻塞失败" << endl;
        return 1;
    }
    int epfd = epoll_create1(0);
    // cout << "epoll_fd = " << epfd << endl;
    if (epfd == -1)
    {
        cerr << "epoll_create1 failed\n";
        close(fds[0]);
        close(fds[1]);
        return 1;
    }

    epoll_event ev{};
    ev.events = EPOLLIN | EPOLLET;
    ev.data.fd = fds[1];

    if (epoll_ctl(epfd, EPOLL_CTL_ADD, fds[1], &ev) == -1)
    {
        cerr << "epoll_ctl failed\n";
        close(epfd);
        close(fds[0]);
        close(fds[1]);
        return 1;
    }

    const char *msg = "aabbcc";
    send(fds[0], msg, 6, 0);

    epoll_event events[10]{};

    int n1 = epoll_wait(epfd, events, 10, 3000);
    cout << "第一次epoll wait：" << n1 << endl;
    if (n1 != 1)
    {
        cerr << "没有收到预期的可读条件" << endl;
        return 1;
    }
    char buffer[16]{};
    ssize_t n = recv(fds[1], buffer, 3, 0);
    cout << "读取字节数：" << n << endl;
    // 再次等待事件
    int n2 = epoll_wait(epfd, events, 10, 200);
    cout << "第二次 epoll_wait:" << n2 << endl;
    char remaining[16]{};
    ssize_t n3 = recv(fds[1], remaining, sizeof(remaining), 0);
    cout << "继续读取：" << n3 << "字节" << endl;
    ssize_t n4 = recv(fds[1], remaining, sizeof(remaining), 0);
    if (n4 == -1 &&
        (errno == EAGAIN || errno == EWOULDBLOCK))
    {
        cout << "EAGAIN: 当前没有更多数据" << endl;
    }
    close(epfd);
    close(fds[0]);
    close(fds[1]);

    return 0;
}
