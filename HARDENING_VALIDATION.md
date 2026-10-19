# Local hardening validation

Date: 2026-10-19. Base checkout: `285cbc5ce78e05b0c6045cba83880c8d99532a86`, with the local hardening changes described in [HARDENING.md](HARDENING.md). Environment: Debian 13 ARM64, GCC 14.2.0. These results do not establish production readiness or support on other platforms.

## Checks

| Command/check | Observed result |
| --- | --- |
| Make C++ unit suite | 137 passed, 0 failed |
| Complete `make -j2 test` | Exit 0; 137 unit tests, 13 CLI checks, 4 error checks, and all 19 new regression tests passed |
| CMake Release build | Built compiler, language server, and C++ test runner successfully |
| CMake integration CTest selection | CLI, errors, and all 19 new regression tests passed (3 CTest entries) |
| Existing native equivalence suite | 13 examples passed |
| Existing native output-path security suite | Both checks passed |
| Existing HTTP server regression suite | All 7 checks passed |
| Embedded native runtime with UBSan | Passed tagged map/array/closure, cycle rendering, bounds-error and integer-overflow checks |
| Embedded native runtime with ASan+UBSan | ASan could not initialize its allocator address space in this environment; this is not a passing ASan result |
| Python syntax and Git whitespace checks | Passed |

The CMake integration command selected `ctest -E '^unit$'`: the C++ unit suite was already run through Make. The new 19-case regression suite exercises subprocess exit codes and outputs, direct/native equivalence, captured cells, loop scope, short-circuit values, formatting, REPL lifetime, static errors, explicit unsupported-feature diagnostics, actual default-build execution, and HTTP argument handling with fake local curl.

## Reproduction commands

```sh
make -j2 test
cmake -S . -B build-cmake -DCMAKE_BUILD_TYPE=Release
cmake --build build-cmake -j2
ctest --test-dir build-cmake --output-on-failure
python3 tests/native_verify/run_all.py
python3 tests/test_native_security.py
python3 tests/test_http_hardening.py
NV_SANITIZERS=undefined python3 tests/test_native_runtime_sanitizers.py
```

The default sanitizer-script command requests ASan+UBSan. This environment rejected ASan during startup before executing test code; the explicitly selected UBSan-only run succeeded. The added GitHub Actions workflow includes sanitizer checks for a normal Ubuntu runner, but no remote CI execution or publishing was performed during this work.

## Test quality corrections

The strengthened standard-library helper exposed a cube-root test that compared floating-point values using exact equality. It now uses a 1e-12 tolerance. The GPU linear-training test previously depended on random starting weights; it now uses fixed starting weights with the original loss threshold. Execution failures and failure markers now fail the standard-library tests.

## Limits

No general benchmark result, sandbox guarantee, memory-safety proof, leak-free guarantee, live PostgreSQL/registry verification, Windows/macOS validation, or production load validation is claimed. Persistent bytecode artifacts are explicitly unavailable. Native compilation deliberately rejects some interpreter features. Reference cycles and broader arithmetic/resource/type-system behavior need further work; see [CURRENT_STATUS.md](CURRENT_STATUS.md).
