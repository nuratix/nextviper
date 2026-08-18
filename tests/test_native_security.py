#!/usr/bin/env python3
import subprocess
import os
import sys
import shutil

print("=== Starting NextViper Native Compiler Security Regression Tests ===")

all_passed = True

# 1. Output path with spaces
test_dir_spaces = "/tmp/nv test space dir"
os.makedirs(test_dir_spaces, exist_ok=True)
bin_spaces = os.path.join(test_dir_spaces, "my compiled binary")

src_code = "/tmp/nv_security_test.nv"
with open(src_code, "w") as f:
    f.write("""
print("SAFE_EXECUTION_SUCCESS_42")
""")

try:
    res = subprocess.run(["/root/nextviper/bin/nextviper", "build", "--native", src_code, "-o", bin_spaces], capture_output=True, text=True)
    assert res.returncode == 0, f"Compilation failed: {res.stderr}"
    assert os.path.exists(bin_spaces), f"Binary not created at {bin_spaces}"
    
    # Run the binary with spaces in path
    run_res = subprocess.run([bin_spaces], capture_output=True, text=True)
    assert "SAFE_EXECUTION_SUCCESS_42" in run_res.stdout
    print("  ✓ PASS: Native binary compiled and executed at path with spaces")
except Exception as e:
    print(f"  ✗ FAIL: Path with spaces: {e}")
    all_passed = False

# 2. Path with Shell Metacharacters (verifying no shell injection occurs)
marker_file = "/tmp/nv_shell_injection_marker.txt"
if os.path.exists(marker_file):
    os.remove(marker_file)

test_dir_meta = "/tmp/nv_meta_test;touch " + marker_file + ";"
os.makedirs(test_dir_meta, exist_ok=True)
bin_meta = os.path.join(test_dir_meta, "prog;id;")

try:
    res = subprocess.run(["/root/nextviper/bin/nextviper", "build", "--native", src_code, "-o", bin_meta], capture_output=True, text=True)
    assert res.returncode == 0, f"Compilation failed: {res.stderr}"
    assert not os.path.exists(marker_file), "SECURITY FLAW: Shell command injection succeeded via compiler output path!"
    
    run_res = subprocess.run([bin_meta], capture_output=True, text=True)
    assert "SAFE_EXECUTION_SUCCESS_42" in run_res.stdout
    print("  ✓ PASS: Shell metacharacter injection blocked (safe posix_spawnp parameter passing)")
except Exception as e:
    print(f"  ✗ FAIL: Shell metacharacters: {e}")
    all_passed = False

# Cleanup
shutil.rmtree(test_dir_spaces, ignore_errors=True)
shutil.rmtree(test_dir_meta, ignore_errors=True)
if os.path.exists(src_code):
    os.remove(src_code)
if os.path.exists(marker_file):
    os.remove(marker_file)

if all_passed:
    print("🎉 ALL NATIVE COMPILER SECURITY REGRESSION TESTS PASSED!")
    sys.exit(0)
else:
    print("❌ SOME NATIVE COMPILER SECURITY REGRESSION TESTS FAILED!")
    sys.exit(1)
