import socket

HOST = "127.0.0.1"
PORT = 6666  # 根据你的实际监听端口修改

def test(request, expected):
    with socket.create_connection((HOST, PORT), timeout=5) as s:
        s.sendall(request)
        s.shutdown(socket.SHUT_WR)

        response = b""
        while True:
            data = s.recv(4096)
            if not data:
                break
            response += data

        status = response.split(b"\r\n", 1)[0].decode()
        print(status)
        assert expected in status

# 测试 1：缺少 Content-Length → 411
test(
    b"POST /infer HTTP/1.1\r\n"
    b"Host: localhost\r\n"
    b"Connection: close\r\n\r\n",
    "411 Length Required"
)

# 测试 2：Content-Length 格式非法 → 400
test(
    b"POST /infer HTTP/1.1\r\n"
    b"Host: localhost\r\n"
    b"Content-Length: abc\r\n"
    b"Connection: close\r\n\r\n",
    "400 Bad Request"
)

print("All tests PASS!")