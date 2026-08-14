# Python Data Benchmark
import time
import math

def run_array_bench():
    t0 = time.perf_counter()
    arr = [float(i) for i in range(100000)]
    s = sum(arr)
    m = s / len(arr)
    mx = max(arr)
    mn = min(arr)
    var = sum((x - m) ** 2 for x in arr) / (len(arr) - 1)
    sd = math.sqrt(var)
    rng = mx - mn if (mx - mn) != 0 else 1.0
    norm = [(x - mn) / rng for x in arr]
    t1 = time.perf_counter()
    print(f"Array 100k items time: {t1 - t0:.6f} sec | Mean: {m} Sum: {s} Std: {sd}")

def run_dataframe_bench():
    t0 = time.perf_counter()
    arr = [[0.0] * 4 for _ in range(10000)]
    t1 = time.perf_counter()
    print(f"DataFrame 10k rows allocation time: {t1 - t0:.6f} sec")

def main():
    print("=== Python NumPy Data Benchmark ===")
    run_array_bench()
    run_dataframe_bench()
    print("Python Benchmark Complete.")

if __name__ == "__main__":
    main()
