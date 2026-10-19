# Testing NextViper

```sh
make test
# Or:
cmake -S . -B build-cmake -DCMAKE_BUILD_TYPE=Release
cmake --build build-cmake -j2
ctest --test-dir build-cmake --output-on-failure
```

The suite includes C++ unit tests, CLI/error checks, and Python subprocess regressions (`tests/test_regressions.py`). Existing native and HTTP server suites can also be run:

```sh
python3 tests/native_verify/run_all.py
python3 tests/test_native_security.py
python3 tests/test_http_hardening.py
```

## Writing .nv tests

A test must assert or fail with a nonzero exit status. Printing the word FAIL alone does not fail a test.

```nextviper
fn add(a, b) => a + b
assert(add(10, 20) == 30, "addition returned an unexpected result")
```

Save this as `tests/test_add.nv` and run `nextviper test tests/test_add.nv`. Directory discovery currently includes all `.nv` files, not only filenames with a test prefix/suffix. Some repository `.nv` examples require an external service or hardware; `make test` is the primary source validation command.

Tests should assert outcomes and error behavior, not just successful parsing or a success message. Standard-library helpers check execution errors and failure markers. Regressions compare direct/native outputs, isolate crash cases in subprocesses, and use fake local curl for shell-injection checks.

## Sanitizer builds

On a supported GCC/Clang Linux toolchain:

```sh
cmake -S . -B build-sanitize -DCMAKE_BUILD_TYPE=Debug -DNEXTVIPER_SANITIZERS=ON
cmake --build build-sanitize -j2
ASAN_OPTIONS=detect_leaks=0 ctest --test-dir build-sanitize --output-on-failure
```

Leak detection is disabled in this example because reference cycles are a known unresolved limitation; this command does not claim leak-free execution. Native generated executables need their own sanitizer flags when investigating native runtime code. Test results are scoped to the cases, platform and compiler actually exercised.

The embedded native runtime can also be checked independently:

```sh
python3 tests/test_native_runtime_sanitizers.py
# If AddressSanitizer cannot initialize in your environment:
NV_SANITIZERS=undefined python3 tests/test_native_runtime_sanitizers.py
```

A sanitizer startup failure is an environment limitation, not a passing memory-safety test. The second command exercises undefined-behavior instrumentation only.
