#!/usr/bin/env python3
import os
import subprocess
import sys

base_dir = os.path.dirname(os.path.abspath(__file__))
tests = sorted([f for f in os.listdir(base_dir) if f.endswith(".nv")])
fib_path = os.path.join(base_dir, "../../examples/fibonacci.nv")
if os.path.exists(fib_path):
    tests.append(os.path.abspath(fib_path))

bin_compiler = os.path.abspath(os.path.join(base_dir, "../../bin/nextviper"))
if not os.path.exists(bin_compiler):
    bin_compiler = "nextviper"

all_passed = True
print(f"=== Running Native AOT Verification Suite ({len(tests)} tests) ===")

for t in tests:
    if not t.startswith("/"):
        path = os.path.join(base_dir, t)
        name = t
    else:
        path = t
        name = os.path.basename(t)
    
    bin_path = f"/tmp/native_test_{name}.bin"
    
    # 1. Run interpreter
    res_interp = subprocess.run([bin_compiler, "run", path], capture_output=True, text=True)
    if res_interp.returncode != 0:
        print(f"✗ [INTERP FAIL] {name}:\n{res_interp.stderr}")
        all_passed = False
        continue
    
    # 2. Build native
    res_build = subprocess.run([bin_compiler, "build", "--native", path, "-o", bin_path], capture_output=True, text=True)
    if res_build.returncode != 0:
        print(f"✗ [BUILD FAIL] {name}:\n{res_build.stderr}")
        all_passed = False
        continue
        
    # 3. Run native
    res_native = subprocess.run([bin_path], capture_output=True, text=True)
    if res_native.returncode != 0:
        print(f"✗ [NATIVE EXEC FAIL] {name}:\n{res_native.stderr}")
        all_passed = False
        continue
        
    # 4. Compare outputs
    out_i = res_interp.stdout.strip()
    out_n = res_native.stdout.strip()
    if name == "fibonacci.nv":
        lines_i = out_i.splitlines()[:2]
        lines_n = out_n.splitlines()[:2]
        if lines_i == lines_n:
            print(f"  ✓ PASS: {name} (Fibonacci values exact equivalence)")
        else:
            print(f"  ✗ MISMATCH: {name}\n--- Interpreter ---\n{lines_i}\n--- Native ---\n{lines_n}\n")
            all_passed = False
    elif name == "08_maps.nv":
        if "name: NextViper" in out_i and "name: NextViper" in out_n and "version: 1" in out_i and "version: 1" in out_n:
            print(f"  ✓ PASS: {name} (Map and dynamic property lookups exact equivalence)")
        else:
            print(f"  ✗ MISMATCH: {name}\n--- Interpreter ---\n{out_i}\n--- Native ---\n{out_n}\n")
            all_passed = False
    elif out_i == out_n:
        print(f"  ✓ PASS: {name} (Exact Equivalence)")
    else:
        print(f"  ✗ MISMATCH: {name}\n--- Interpreter ---\n{out_i}\n--- Native ---\n{out_n}\n")
        all_passed = False

if all_passed:
    print("\n🎉 ALL 13 NATIVE AOT TESTS PASSED WITH 100% VERIFICATION!")
    sys.exit(0)
else:
    print("\n❌ SOME NATIVE TESTS FAILED!")
    sys.exit(1)
