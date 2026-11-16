
import socket
import time
import urllib.request

HOST = "127.0.0.1"
PORT = 6666
SLOW_CLIENTS = 4

sockets = []

try:
    # 1. 建立四个慢客户端连接
    for i in range(SLOW_CLIENTS):
        sock = socket.create_connection(
            (HOST, PORT),
            timeout=3
        )

        # 发送不完整的 HTTP 请求头
        sock.sendall(
            b"POST /infer HTTP/1.1\r\n"
            b"Host: 127.0.0.1:9090\r\n"
            b"Content-Length: 10\r\n"
        )

        sockets.append(sock)
        print(f"Slow client {i + 1} connected")

    # 给服务器时间处理这四个连接
    time.sleep(1)

    print("\nSending normal GET /health ...")

    # 2. 测试普通客户端能否得到响应
    start = time.perf_counter()

    try:
        with urllib.request.urlopen(
            f"http://{HOST}:{PORT}/health",
            timeout=3
        ) as resp:
            body = resp.read().decode()
            elapsed = time.perf_counter() - start

            print("Status:", resp.status)
            print("Body:", body)
            print(f"Elapsed: {elapsed:.3f}s")

    except Exception as e:
        elapsed = time.perf_counter() - start
        print("Normal request failed:", repr(e))
        print(f"Elapsed: {elapsed:.3f}s")

finally:
    # 3. 关闭慢客户端，释放连接
    for sock in sockets:
        sock.close()

    print("\nAll slow clients closed")
