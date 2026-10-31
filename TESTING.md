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

## Engineering contract and examples

`tests/test_language_contract.py` compares expected stdout, empty stderr and
successful status for explicitly selected backends. Error tests assert failure
status and diagnostic content where formatting differs across engines. The slice
matrix checks 216 boundary/stride combinations in each of the interpreter and VM
against Python's slice reference. Native slicing, VM captures/imports, and
language-level exceptions are not counted as supported cases. Function return
values are observed through printed calls; there is no separate process return
value protocol.

`tests/native_verify/run_all.py` compares complete native/interpreter stdout,
stderr and exit status for 13 fixtures. Only Fibonacci's measured elapsed-time
number is normalized; the line must still be present and well-formed. Temporary
executables use private directories, and every subprocess has a timeout.
`NEXTVIPER_BIN` selects the compiler for both suites.

Practical applications and grammar metadata/pattern checks run in Make and CTest.
The grammar checks do not run a TextMate engine. See the
[troubleshooting guide](docs/troubleshooting.md) and [validation record](docs/audits/nextviper-validation.md).
