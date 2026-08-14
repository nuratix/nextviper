import time
import json
import hashlib

start_time = time.time()

# 1. JSON Stringify & Parse 1,000 iterations
sample_obj = {
    "id": 1001,
    "user": "NextViper_Benchmark_Runner",
    "metrics": [10, 20, 30, 40, 50],
    "active": True
}

for _ in range(1000):
    text = json.dumps(sample_obj)
    parsed = json.loads(text)

# 2. Crypto SHA-256 & MD5 5,000 iterations
payload = b"benchmark_payload_string_nextviper_high_performance"
for _ in range(5000):
    h256 = hashlib.sha256(payload).hexdigest()
    hmd5 = hashlib.md5(payload).hexdigest()

# 3. String transformations 5,000 iterations
for _ in range(5000):
    s = "high performance standard library".upper()
    parts = s.split(" ")
    joined = "-".join(parts)

# 4. Collections Chunk & Sort
numbers = [99, 42, 10, 88, 5, 23, 76, 12, 64, 31, 55, 18, 90, 3, 47]
for _ in range(1000):
    srt = sorted(numbers)
    chunks = [srt[i:i+4] for i in range(0, len(srt), 4)]

elapsed = time.time() - start_time
print("STDLIB_BENCHMARK_COMPLETED")
print(elapsed)
