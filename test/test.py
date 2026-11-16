import socket

with socket.create_connection(("127.0.0.1", 9090)) as s:
    s.sendall(
        b"GET /health HTTP/1.1\r\n"
        + b"X-Test: "
        + b"A" * 9000
    )
    s.shutdown(socket.SHUT_WR)