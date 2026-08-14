# Benchmark 06: Maps - Hash map insertions, lookups, and aggregation in Python
def map_benchmark(n):
    m = {}
    i = 0
    while i < n:
        key = str(i % 500)
        m[key] = (i * 3) + 7
        i += 1
    
    total = 0
    j = 0
    while j < 500:
        k = str(j)
        total += m[k]
        j += 1
    return total

total = map_benchmark(5000)
print(total)
