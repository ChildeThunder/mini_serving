// 网络服务层
// 负责监听端口、接受客户端连接、收发数据，以及调用 HTTP 和业务处理函数。

#pragma once
#include <cstddef>
#include <cstdint>
#include "ThreadPool.h"
constexpr size_t MAX_HEADER_SIZE = 8 * 1024;
constexpr size_t MAX_BODY_SIZE = 4 * 1024;

class HttpServer
{
public:
    explicit HttpServer(uint16_t port);

    ~HttpServer();

    // 不允许拷贝服务器对象
    HttpServer(const HttpServer &) = delete;
    HttpServer &operator=(const HttpServer &) = delete;

    // 启动服务器，成功退出返回 0
    int run();

private:
    uint16_t port_;
    int listen_fd_ = -1;
    ThreadPool pool_;
    // 创建、绑定并监听 Socket
    bool init_socket();

    // 处理单个客户端连接
    void handle_client(int client_fd);
};
bool send_all(int fd, const char *data, size_t len);
bool recv_exact(int fd, char *buffer, size_t len);