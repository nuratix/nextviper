# Benchmark 05: Lists - Dynamic array building, indexing and accumulation in Python
def list_benchmark(n):
    lst = []
    i = 0
    while i < n:
        lst.append(i * 2 + 1)
        i += 1
    
    total = 0
    j = 0
    list_len = len(lst)
    while j < list_len:
        total += lst[j]
        j += 1
    return total

total = list_benchmark(10000)
print(total)
