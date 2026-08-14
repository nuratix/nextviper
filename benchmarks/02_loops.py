# Benchmark 02: Loops - Nested iteration and accumulator in Python
def compute_loops(outer, inner):
    total = 0
    i = 0
    while i < outer:
        j = 0
        while j < inner:
            total = (total + (i * j) % 97) % 1000003
            j += 1
        i += 1
    return total

result = compute_loops(2000, 500)
print(result)
