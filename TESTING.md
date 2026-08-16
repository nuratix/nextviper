# NextViper Automated Testing Framework

NextViper includes an integrated test runner (`nextviper test`) for unit tests, integration tests, and regression suites.

---

## 1. Test Discovery Convention

By convention, test files are placed in the `tests/` directory and named with a `_test.nv` suffix or `test_*.nv` prefix:

```
my_project/
├── src/
│   ├── math.nv
│   └── server.nv
└── tests/
    ├── math_test.nv
    └── server_test.nv
```

---

## 2. Writing Tests

A NextViper test file imports the target module, executes logic, and asserts invariants:

```nextviper
// tests/math_test.nv
import std.io

fn test_addition():
    let res = 10 + 20
    if res != 30:
        io.print("FAIL: Expected 30, got " + string(res))

fn test_array_operations():
    let arr = [1, 2, 3]
    if len(arr) != 3:
        io.print("FAIL: Array length mismatch")

test_addition()
test_array_operations()
```

---

## 3. Running Tests

```bash
# Discover and run all tests in tests/
nextviper test

# Run a specific test file
nextviper test tests/math_test.nv
```

### Output Format
```
====================================================
  NextViper Test Runner (2 test file(s))
====================================================

  ✓ PASS: tests/math_test.nv (1.14 ms)
  ✓ PASS: tests/server_test.nv (2.45 ms)

----------------------------------------------------
Tests Summary: 2 total | 2 passed | 0 failed (3.59 ms)
====================================================
```
