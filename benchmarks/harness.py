#!/usr/bin/env python3
import time
import subprocess
import os
import statistics
import json

BENCHMARKS = [
    ("01_arithmetic", "Arithmetic (Prime Sieve & Numeric Series)", "benchmarks/01_arithmetic.nv", "benchmarks/01_arithmetic.py", True),
    ("02_loops", "Loops (Nested 1M Iterations Accumulation)", "benchmarks/02_loops.nv", "benchmarks/02_loops.py", True),
    ("03_function_calls", "Function Calls (Recursive Fib(28) 1M frames)", "benchmarks/03_function_calls.nv", "benchmarks/03_function_calls.py", True),
    ("04_strings", "Strings (Concat, Slicing, Length Checks)", "benchmarks/04_strings.nv", "benchmarks/04_strings.py", False),
    ("05_lists", "Lists (Dynamic 10k Elements & Indexing)", "benchmarks/05_lists.nv", "benchmarks/05_lists.py", False),
    ("06_maps", "Maps (Hash Map 5k Insertions & Lookups)", "benchmarks/06_maps.nv", "benchmarks/06_maps.py", False),
    ("07_file_processing", "File Processing (CSV Load, Clean, Split)", "benchmarks/07_file_processing.nv", "benchmarks/07_file_processing.py", False),
    ("08_data_processing", "Data Processing (60x60 Matmul & Reductions)", "benchmarks/08_data_processing.nv", "benchmarks/08_data_processing.py", False),
]

NEXTVIPER_BIN = "./bin/nextviper"
ITERATIONS = 5

def measure_cmd(cmd):
    # Warm up
    try:
        subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, check=True)
    except Exception as e:
        return None

    times = []
    for _ in range(ITERATIONS):
        t0 = time.perf_counter()
        res = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        t1 = time.perf_counter()
        if res.returncode == 0:
            times.append((t1 - t0) * 1000.0) # ms
        else:
            return None
    if not times:
        return None
    return {
        "mean": statistics.mean(times),
        "median": statistics.median(times),
        "stdev": statistics.stdev(times) if len(times) > 1 else 0.0,
        "min": min(times),
        "max": max(times)
    }

def main():
    print(f"Running NextViper Comprehensive Benchmark Suite ({ITERATIONS} runs per test)...\n")
    results = []

    for name, title, nv_file, py_file, supports_native in BENCHMARKS:
        print(f"--> Benchmarking: {title}")

        # 1. NextViper Interpreter (Tree-Walk)
        interp_res = measure_cmd([NEXTVIPER_BIN, "run", nv_file, "--tree"])
        print(f"    • NextViper Interpreter: {interp_res['median']:.2f} ms" if interp_res else "    • NextViper Interpreter: N/A")

        # 2. NextViper Bytecode VM
        vm_res = measure_cmd([NEXTVIPER_BIN, "run", nv_file])
        print(f"    • NextViper VM:          {vm_res['median']:.2f} ms" if vm_res else "    • NextViper VM: N/A")

        # 3. NextViper Native Compiled Binary
        native_bin = f"/tmp/{name}_native_bin"
        native_res = None
        if supports_native:
            compile_cmd = [NEXTVIPER_BIN, "compile", nv_file, "-o", native_bin, "--release"]
            compile_proc = subprocess.run(compile_cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
            if compile_proc.returncode == 0:
                native_res = measure_cmd([native_bin])
                print(f"    • NextViper Native AOT:  {native_res['median']:.2f} ms" if native_res else "    • NextViper Native AOT: N/A")
                if os.path.exists(native_bin):
                    os.remove(native_bin)
            else:
                print("    • NextViper Native AOT: Compilation skipped")
        else:
            print("    • NextViper Native AOT: Tier 2 (VM/Host standard library)")

        # 4. Python 3
        py_res = measure_cmd(["python3", py_file])
        print(f"    • Python 3.14:           {py_res['median']:.2f} ms\n" if py_res else "    • Python 3: N/A\n")

        results.append({
            "id": name,
            "title": title,
            "interp": interp_res,
            "vm": vm_res,
            "native": native_res,
            "python": py_res
        })

    with open("benchmarks/results.json", "w") as f:
        json.dump(results, f, indent=2)

    print("✓ Benchmarks completed. Results saved to benchmarks/results.json")

if __name__ == "__main__":
    main()
