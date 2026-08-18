#!/usr/bin/env python3
import socket
import subprocess
import time
import os
import sys

print("=== Starting NextViper HTTP Server Hardening Regression Tests ===")

os.makedirs("/tmp/nv_static_test", exist_ok=True)
with open("/tmp/nv_static_test/index.html", "w") as f:
    f.write("<h1>Public Index</h1>")
with open("/tmp/nv_static_test/secret.txt", "w") as f:
    f.write("public secret")
with open("/tmp/secret_outside.txt", "w") as f:
    f.write("TOP_SECRET_CANNOT_ESCAPE")

server_script = "/tmp/nv_harden_server.nv"
with open(server_script, "w") as f:
    f.write("""
import http
import time

let app = http.server()
app.static("/static", "/tmp/nv_static_test")

app.get("/api/echo_query", fn(req) {
    let q = req.query["search"]
    return {
        "status": 200,
        "headers": {"content-type": "application/json"},
        "body": {"received_search": q}
    }
})

app.post("/api/echo_body", fn(req) {
    let j = req.json()
    return {
        "status": 201,
        "body": j
    }
})

app.listen(8990, "127.0.0.1", true)
time.sleep(10000)
""")

proc = subprocess.Popen(["/root/nextviper/bin/nextviper", "run", server_script], stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
time.sleep(0.5)

all_passed = True

def send_raw(payload, timeout=2.0):
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.settimeout(timeout)
    s.connect(("127.0.0.1", 8990))
    try:
        s.sendall(payload)
    except Exception:
        pass
    resp = b""
    try:
        while True:
            chunk = s.recv(4096)
            if not chunk: break
            resp += chunk
    except Exception:
        pass
    s.close()
    return resp.decode('utf-8', errors='ignore')

# 1. Path Traversal Test (plain ../../)
try:
    resp = send_raw(b"GET /static/../../secret_outside.txt HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n")
    assert "404 Not Found" in resp or "400 Bad Request" in resp or len(resp) == 0
    assert "TOP_SECRET_CANNOT_ESCAPE" not in resp, "Leaked file outside static root!"
    print("  ✓ PASS: Path Traversal (../../) Blocked")
except Exception as e:
    print(f"  ✗ FAIL: Path Traversal: {e}")
    all_passed = False

# 2. Encoded Traversal Test (%2e%2e%2f)
try:
    resp = send_raw(b"GET /static/%2e%2e/%2e%2e/secret_outside.txt HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n")
    assert "TOP_SECRET_CANNOT_ESCAPE" not in resp, "Encoded path traversal leaked secret!"
    assert "404 Not Found" in resp or "400 Bad Request" in resp or len(resp) == 0
    print("  ✓ PASS: Encoded Traversal (%2e%2e%2f) Blocked")
except Exception as e:
    print(f"  ✗ FAIL: Encoded Traversal: {e}")
    all_passed = False

# 3. URL-Decoded Query Parameters
try:
    resp = send_raw(b"GET /api/echo_query?search=Hello%20World%21 HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n")
    assert "200 OK" in resp
    assert "Hello World!" in resp
    print("  ✓ PASS: URL-Decoded Query Parameters Handled")
except Exception as e:
    print(f"  ✗ FAIL: URL Decoding: {e}")
    all_passed = False

# 4. Malformed HTTP Request Line
try:
    resp = send_raw(b"INVALID_PROTOCOL_JUNK\r\n\r\n")
    assert "400 Bad Request" in resp or len(resp) == 0
    print("  ✓ PASS: Malformed Request Line Handled (400 Bad Request / Closed)")
except Exception as e:
    print(f"  ✗ FAIL: Malformed Request: {e}")
    all_passed = False

# 5. Oversized Header (>16KB)
try:
    huge_header = b"GET /api/echo_query HTTP/1.1\r\nX-Huge: " + (b"A" * (18 * 1024)) + b"\r\n\r\n"
    resp = send_raw(huge_header)
    assert "431" in resp or len(resp) == 0 or "Request Header Fields Too Large" in resp
    print("  ✓ PASS: Oversized Headers Rejected (>16KB limit / 431)")
except Exception as e:
    print(f"  ✗ FAIL: Oversized Headers: {type(e)} {e}")
    all_passed = False

# 6. Fragmented TCP Request
try:
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.connect(("127.0.0.1", 8990))
    s.send(b"POST /api/echo_body HTTP/1.1\r\nHost: localhost\r\nContent-Type: application/json\r\nContent-Length: 14\r\n")
    time.sleep(0.05)
    s.send(b"\r\n")
    time.sleep(0.05)
    s.send(b'{"msg":')
    time.sleep(0.05)
    s.send(b'"ok"}')
    resp = s.recv(4096).decode('utf-8', errors='ignore')
    s.close()
    assert "201 Created" in resp
    assert "ok" in resp
    print("  ✓ PASS: Fragmented TCP Request Handled Correctly")
except Exception as e:
    print(f"  ✗ FAIL: Fragmented Request: {e}")
    all_passed = False

# 7. Reason Phrases Verification
try:
    resp_200 = send_raw(b"GET /static/index.html HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n")
    assert "HTTP/1.1 200 OK" in resp_200
    resp_404 = send_raw(b"GET /not_found_route HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n")
    assert "HTTP/1.1 404 Not Found" in resp_404
    print("  ✓ PASS: Standard HTTP Status Reason Phrases (200 OK, 404 Not Found)")
except Exception as e:
    print(f"  ✗ FAIL: Reason phrases: {e}")
    all_passed = False

proc.terminate()

if all_passed:
    print("🎉 ALL HTTP HARDENING REGRESSION TESTS PASSED!")
    sys.exit(0)
else:
    print("❌ SOME HTTP HARDENING REGRESSION TESTS FAILED!")
    sys.exit(1)
