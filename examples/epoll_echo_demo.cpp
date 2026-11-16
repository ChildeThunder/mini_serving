#include <iostream>
#include <string>
#include <cstring>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <cerrno>

using namespace std;
int main()
{
    // 1.创建监听套结字，获得文件描述符
    int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd == -1)
    {
        cerr << "创建监听套结字失败，程序结束" << endl;
        return 1;
    }

    int opt = 1;
    // 2.允许服务器在特定情况下能够重新绑定之前使用过的IP地址和端口
    int res = setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    if (res == -1)
    {
        cerr << strerror(errno) << endl;
        close(listen_fd);
        return 1;
    }

    // 3.设置服务器地址
    struct sockaddr_in addr{};
    addr.sin_port = htons(6666); // 小端转大端
    addr.sin_family = AF_INET;
    // 小端转大端
    if (inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr) != 1)
    {
        cerr << "无效的IP地址" << endl;
        close(listen_fd);
        return 1;
    }

    // 4.绑定本地IP和端口
    if (bind(listen_fd, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) == -1)
    {
        cerr << strerror(errno) << endl;
        close(listen_fd);
        return 1;
    }

    // 5.进入TCP监听状态
    if (listen(listen_fd, 128) == -1)
    {
        cerr << strerror(errno) << endl;
        close(listen_fd);
        return 1;
    }

    // 6.设置非阻塞状态
    int flags = fcntl(listen_fd, F_GETFL);
    if (flags == -1 || fcntl(listen_fd, F_SETFL, flags | O_NONBLOCK) == -1)
    {
        cerr << "设置非阻塞失败" << strerror(errno) << endl;
        close(listen_fd);
        return 1;
    }

    // 7.创建并注册epoll
    int epoll_fd = epoll_create1(0);
    if (epoll_fd == -1)
    {
        cerr << strerror(errno) << endl;
        close(listen_fd);
        return 1;
    }
    struct epoll_event listen_ev{};
    listen_ev.events = EPOLLIN;
    listen_ev.data.fd = listen_fd;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, listen_fd, &listen_ev) == -1)
    {
        cerr << "epoll_ctl失败：" << strerror(errno) << endl;
        close(epoll_fd);
        close(listen_fd);
        return 1;
    }
    while (1)
    {
        epoll_event events[100];
        int n = epoll_wait(epoll_fd, events, 100, 3000);
        if (n > 0)
        {
            for (int i = 0; i < n; i++)
            {
                int ready_fd = events[i].data.fd;
                if (ready_fd == listen_fd && (events[i].events & EPOLLIN))
                {
                    // 获取客户端的IP地址和端口
                    sockaddr_in client_addr{};
                    socklen_t client_len = sizeof(client_addr);
                    int client_fd = accept(listen_fd, reinterpret_cast<sockaddr *>(&client_addr), &client_len);
                    if (client_fd >= 0)
                    {
                        char ip[INET_ADDRSTRLEN]{};
                        const char *result = inet_ntop(
                            AF_INET,
                            &client_addr.sin_addr,
                            ip,
                            sizeof(ip));
                        if (result != nullptr)
                        {
                            cout << "客户端IP：" << ip << endl;
                            cout << "客户端端口：" << ntohs(client_addr.sin_port) << endl;
                        }
                        //
                        int flags = fcntl(client_fd, F_GETFL);
                        if (flags == -1 || fcntl(client_fd, F_SETFL, flags | O_NONBLOCK) == -1)
                        {
                            cerr << "client_fd设置非阻塞失败" << strerror(errno) << endl;
                            close(client_fd);
                            continue;
                        }
                        epoll_event client_ev{};
                        client_ev.events = EPOLLIN;
                        client_ev.data.fd = client_fd;
                        if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_fd, &client_ev) == -1)
                        {
                            close(epoll_fd);
                            close(listen_fd);
                            return 0;
                        }
                    }
                    else if (errno == EINTR) // 被信号中断，可以重试
                    {
                        continue;
                    }
                    else if (errno == EAGAIN || errno == EWOULDBLOCK) // 当前没有数据可读，稍后再处理
                    {
                        break;
                    }
                    else
                    {
                        cerr << "accept失败" << strerror(errno) << endl;
                    }
                }
                else if (ready_fd != listen_fd && (events[i].events & EPOLLIN))
                {
                    // 处理客户端数据
                    char buffer[1024];
                    while (1)
                    {
                        ssize_t len = recv(ready_fd, buffer, sizeof(buffer), 0);
                        if (len > 0)
                        {
                            cout << "client_fd=" << ready_fd << " received:" << string(buffer, len) << endl;
                            size_t total_send = 0;
                            bool is_send = true;
                            while (total_send < static_cast<size_t>(len))
                            {
                                ssize_t send_res = send(ready_fd, buffer + total_send, static_cast<size_t>(len) - total_send, MSG_NOSIGNAL);
                                if (send_res > 0)
                                {
                                    total_send += send_res;
                                }
                                else if (send_res == 0)
                                {
                                    cerr << "没有成功发送数据" << endl;
                                    is_send = false;
                                    break;
                                }
                                else if (errno == EINTR)
                                {
                                    // 被信号中断，重试
                                    continue;
                                }
                                else if (errno == EAGAIN || errno == EWOULDBLOCK)
                                {
                                    // 当前发送区没有数据可发送
                                    cerr << "发送缓冲区暂时不可写" << endl;
                                    is_send = false;
                                    break;
                                }
                                else
                                {
                                    cerr << "send失败：" << strerror(errno) << endl;
                                    is_send = false;
                                    break;
                                }
                            }
                            if (is_send == false)
                            {
                                close(ready_fd);
                                break;
                            }
                        }
                        else if (len == 0)
                        {
                            cout << "客户端关闭发送方向，fd=" << ready_fd << endl;
                            close(ready_fd);
                            break;
                        }
                        // 被信号中断
                        else if (errno == EINTR)
                        {
                            continue;
                        }
                        else if (errno == EAGAIN || errno == EWOULDBLOCK)
                        {
                            break;
                        }
                        else
                        {
                            cerr << "recv失败：" << strerror(errno) << endl;
                            close(ready_fd);
                            break;
                        }
                    }
                }
            }
        }
        else if (n == 0)
        {
            // 等待超时
            continue;
        }
        else
        {
            cerr << "等待失败：" << strerror(errno) << endl;
            close(listen_fd);
            return 1;
        }
    }
    close(epoll_fd);
    close(listen_fd);
    return 0;
}