
import socket
import time
import threading
from concurrent.futures import ThreadPoolExecutor

HOST = "127.0.0.1"
PORT = 6666


def http_request(path="/health", body=None, timeout=5):
    with socket.create_connection((HOST, PORT), timeout=timeout) as s:
        if body is None:
            req = (
                f"GET {path} HTTP/1.1\r\n"
                "Host: localhost\r\n"
                "Connection: close\r\n\r\n"
            ).encode()
        else:
            data = body.encode()
            req = (
                f"POST {path} HTTP/1.1\r\n"
                "Host: localhost\r\n"
                f"Content-Length: {len(data)}\r\n"
                "Connection: close\r\n\r\n"
            ).encode() + data

        s.sendall(req)

        response = b""
        while True:
            chunk = s.recv(4096)
            if not chunk:
                break
            response += chunk

        return response


def check(response, expected_body):
    assert b"HTTP/1.1 200 OK" in response
    assert expected_body in response


def slow_client():
    s = socket.create_connection((HOST, PORT), timeout=5)

    # 故意不发送 Header 结束标记
    s.sendall(
        b"GET /health HTTP/1.1\r\n"
        b"Host: localhost\r\n"
    )
    return s


# 测试 A：一个慢客户端不能阻塞其他客户端
print("Test A: Slow client + normal client")

slow = slow_client()
try:
    time.sleep(0.3)

    response = http_request(timeout=2)
    check(response, b'{"status":"ok"}')

    print("Test A: PASS")
finally:
    slow.close()


# 测试 B：四个 Worker 被占满后的任务排队
print("Test B: Worker saturation")

time.sleep(0.3)
slows = []

try:
    for i in range(4):
        slows.append(slow_client())

    time.sleep(1)

    started = threading.Event()

    def fifth_request():
        started.set()
        return http_request(timeout=5)

    with ThreadPoolExecutor(max_workers=1) as pool:
        future = pool.submit(fifth_request)
        assert started.wait(timeout=2)

        time.sleep(0.5)

        # 4 个 Worker 都被占用，第 5 个请求应等待
        assert not future.done(), "请求没有等待，检查 Worker 数量"

        # 释放其中一个 Worker
        slows[0].shutdown(socket.SHUT_WR)

        response = future.result(timeout=5)
        check(response, b'{"status":"ok"}')

    print("Test B: PASS")
finally:
    for s in slows:
        s.close()


# 测试 C：20 个并发正常请求
print("Test C: 20 concurrent requests")

time.sleep(0.3)

with ThreadPoolExecutor(max_workers=10) as pool:
    futures = []

    for i in range(20):
        if i % 2 == 0:
            futures.append(
                pool.submit(http_request, "/health")
            )
        else:
            futures.append(
                pool.submit(http_request, "/infer", "1 2 3 4")
            )

    for i, future in enumerate(futures):
        response = future.result(timeout=8)

        if i % 2 == 0:
            check(response, b'{"status":"ok"}')
        else:
            check(response, b'{"sum":10}')

print("Test C: PASS")
print("All ThreadPool tests PASS!")
