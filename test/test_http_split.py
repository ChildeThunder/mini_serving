
import socket
import time

HOST = "127.0.0.1"
PORT = 6666  # 与 main.cpp 保持一致

with socket.create_connection((HOST, PORT), timeout=5) as s:
    # 第一段：请求行
    s.sendall(b"POST /infer HTTP/1.1\r\n")
    time.sleep(0.05)

    # 第二段：剩余 Header
    s.sendall(
        b"Host: 127.0.0.1\r\n"
        b"Content-Type: text/plain\r\n"
        b"Content-Length: 7\r\n"
        b"Connection: close\r\n"
        b"\r\n"
    )
    time.sleep(0.05)

    # 第三段：部分 Body
    s.sendall(b"1 2 ")
    time.sleep(0.05)

    # 第四段：剩余 Body
    s.sendall(b"3 4")
    s.shutdown(socket.SHUT_WR)

    # 接收完整响应
    response = bytearray()

    while True:
        data = s.recv(4096)
        if not data:
            break
        response.extend(data)

    text = response.decode("utf-8")
    print(text)

    assert "HTTP/1.1 200 OK" in text
    assert '{"sum":10}' in text

print("HTTP split test: PASS")
