import socket
import time

with socket.create_connection(("127.0.0.1", 9090), timeout=5) as s:
    s.sendall(b"POST /infer HTTP/1.1\r\n")
    time.sleep(0.05)

    s.sendall(
        b"Host: 127.0.0.1:9090\r\n"
        b"Content-Type: text/plain\r\n"
        b"Content-Length: 7\r\n"
        b"Connection: close\r\n\r\n"
    )

    time.sleep(0.05)

    s.sendall(b"1 2 ")
    time.sleep(0.05)
    s.sendall(b"3 4")

    s.shutdown(socket.SHUT_WR)

    response = bytearray()

    while True:
        data = s.recv(4096)
        if not data:
            break
        response.extend(data)

    print(response.decode())