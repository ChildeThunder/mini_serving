"""MiniServing HTTP benchmark: standard-library-only, one request per connection."""
import math
import socket
import time
from concurrent.futures import ThreadPoolExecutor, as_completed

HOST = "127.0.0.1"
PORT = 6666
TOTAL_REQUESTS = 1000
CONCURRENCIES = (1, 4, 8, 16)
TIMEOUT_SECONDS = 5


def request(method):
    if method == "GET":
        payload = b""
        expected_body = b'{"status":"ok"}'
        request_bytes = (
            b"GET /health HTTP/1.1\r\n"
            b"Host: localhost\r\n"
            b"Connection: close\r\n\r\n"
        )
    else:
        payload = b"1 2 3 4"
        expected_body = b'{"sum":10}'
        request_bytes = (
            b"POST /infer HTTP/1.1\r\n"
            b"Host: localhost\r\n"
            + f"Content-Length: {len(payload)}\r\n".encode()
            + b"Connection: close\r\n\r\n"
            + payload
        )

    start = time.perf_counter()
    try:
        with socket.create_connection((HOST, PORT), timeout=TIMEOUT_SECONDS) as conn:
            conn.sendall(request_bytes)
            chunks = []
            while True:
                chunk = conn.recv(4096)
                if not chunk:
                    break
                chunks.append(chunk)
        response = b"".join(chunks)
        header, separator, body = response.partition(b"\r\n\r\n")
        status_line = header.split(b"\r\n", 1)[0]
        if not separator or status_line != b"HTTP/1.1 200 OK" or body != expected_body:
            return False, 0.0, f"Unexpected response: {response[:160]!r}"
        return True, (time.perf_counter() - start) * 1000, ""
    except (OSError, TimeoutError) as exc:
        return False, 0.0, str(exc)


def percentile(sorted_values, percent):
    index = math.ceil(len(sorted_values) * percent / 100) - 1
    return sorted_values[max(index, 0)]


def benchmark(method, concurrency):
    # A small warm-up not counted in the measurement.
    for _ in range(10):
        ok, _, error = request(method)
        if not ok:
            print(f"Warm-up failed ({method}): {error}")
            return

    latencies = []
    failures = []
    start = time.perf_counter()
    with ThreadPoolExecutor(max_workers=concurrency) as pool:
        futures = [pool.submit(request, method) for _ in range(TOTAL_REQUESTS)]
        for future in as_completed(futures):
            ok, latency_ms, error = future.result()
            if ok:
                latencies.append(latency_ms)
            else:
                failures.append(error)
    seconds = time.perf_counter() - start

    if not latencies:
        print(f"{method:4} | {concurrency:2} | all failed; first error: {failures[0]}")
        return

    latencies.sort()
    print(
        f"{method:4} | {concurrency:2} | "
        f"{len(latencies):4}/{TOTAL_REQUESTS} | "
        f"{TOTAL_REQUESTS / seconds:8.1f} | "
        f"{sum(latencies) / len(latencies):8.2f} | "
        f"{percentile(latencies, 50):7.2f} | "
        f"{percentile(latencies, 95):7.2f} | "
        f"{percentile(latencies, 99):7.2f} | "
        f"{len(failures):3}"
    )
    if failures:
        print("  First failure:", failures[0])


if __name__ == "__main__":
    print("Test: one HTTP request per TCP connection; latency includes connection setup")
    print("HTTP | C  | OK/ALL    |      QPS | mean(ms) | P50(ms) | P95(ms) | P99(ms) | ERR")
    for method in ("GET", "POST"):
        for concurrency in CONCURRENCIES:
            benchmark(method, concurrency)
