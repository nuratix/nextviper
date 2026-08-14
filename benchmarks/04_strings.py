# Benchmark 04: Strings - Concatenation, slicing and transformations in Python
def string_benchmark(iterations):
    s = ""
    i = 0
    while i < iterations:
        s = s + "NextViper"
        if len(s) > 100:
            s = s[0:50]
        i += 1
    return len(s)

final_len = string_benchmark(2000)
print(final_len)
