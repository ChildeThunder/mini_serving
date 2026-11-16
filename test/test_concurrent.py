
import concurrent.futures
import time
import urllib.request

URL = "http://127.0.0.1:6666/infer"
REQUESTS = 12
WORKERS = 12

def send_request(i):
    data = f"{i} 2 3".encode("utf-8")

    req = urllib.request.Request(
        URL,
        data=data,
        method="POST"
    )

    start = time.perf_counter()

    try:
        with urllib.request.urlopen(req, timeout=10) as resp:
            body = resp.read().decode("utf-8")
            elapsed = time.perf_counter() - start

            return (
                i,
                resp.status,
                body,
                elapsed
            )
    except Exception as e:
        return i, "ERROR", str(e), 0

start_total = time.perf_counter()

with concurrent.futures.ThreadPoolExecutor(
    max_workers=WORKERS
) as executor:
    results = list(executor.map(
        send_request,
        range(REQUESTS)
    ))

total = time.perf_counter() - start_total

for i, status, body, elapsed in results:
    print(
        f"Request {i:02d} | "
        f"Status: {status} | "
        f"Time: {elapsed:.3f}s | "
        f"Body: {body}"
    )

print(f"\nTotal time: {total:.3f}s")
