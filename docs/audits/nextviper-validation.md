# NextViper validation record

Date: 2026-10-31. Base: `bcf1394`. Correctness commit: `544fced`.
The local platform is Debian 13 Linux ARM64 with GCC 14.2, CMake 3.31.6,
Python 3.13.5, Vulkan headers/loader 1.4.309, libpq 17.11 and libcurl 8.14.1.
AI-assisted work; results establish the exercised cases only.

## Build and suite evidence

The environment initially lacked CMake and development headers. Those build
requirements were installed. A Release CMake build produced the compiler,
language server and unit runner. `ctest --test-dir build-cmake --output-on-failure -j2`
completed: **11/11 groups passed, 0 failed**, in 414.66 seconds. The completed
[captured CTest record](artifacts/release-ctest.txt) contains individual results. Captured text logs normalize ANSI color escapes
and trailing whitespace; benchmark JSON retains every measured sample.

The C++ runner was rebuilt during that broad run to add explicit GPU skip
reporting. Its [separate final rerun](artifacts/final-unit.txt) passed in 204.34
seconds: **141 passed, 0 failed, 0 skipped**. Vulkan was available through
**llvmpipe (LLVM 19.1.7, 128 bits)**, a software implementation; this validates
those software Vulkan paths, not physical GPU hardware.

Reconciled case counts for the main CTest C++/Python suites: **203 total,
200 passed, 0 failed, 3 skipped**. The two shell scripts are additional passing
groups. Native fixture/security and sanitizer runs below are reported separately
to avoid double-counting overlapping unit coverage.

| Check | Completed result |
| --- | --- |
| Release CMake build | Passed (compiler, LSP and C++ runner) |
| Broad CTest | 11 passed groups, 0 failed groups; database cases skip |
| Final C++ unit rerun | 141 passed, 0 failed, 0 skipped; software Vulkan available |
| Missing-device GPU subset | 3 passed, 0 failed, 5 explicitly skipped; deliberately absent Vulkan driver |
| Python regressions | 20 passed |
| Transport contracts | 6 passed |
| Language contract | 15 passed; includes 216 slices per interpreter/VM |
| HTTP hardening | 9 passed, real loopback sockets |
| PostgreSQL backend | 3 skipped: no dedicated test database |
| Distribution tests | 2 passed |
| Practical applications | 4 passed (calculator cases, errors, text and real HTTP) |
| Grammar metadata/patterns | 3 passed; no TextMate engine certification |
| CLI and error scripts | Both passed; reported as groups, not a fabricated case count |
| Native equivalence | 13 fixtures passed (one unittest with fixture subtests) |
| Native path security | 1 test passed with 3 output-path subtests |
| Front-end UBSan | 20 lexer/parser tests passed, 0 failed, 0 skipped |
| Embedded native runtime UBSan | Passed |
| ASan | Blocked before tests by allocator initialization failure |

Commands for standalone checks:

```sh
NEXTVIPER_BIN="$PWD/build-cmake/bin/nextviper" python3 tests/native_verify/run_all.py
NEXTVIPER_BIN="$PWD/build-cmake/bin/nextviper" python3 tests/test_native_security.py
NV_SANITIZERS=undefined python3 tests/test_native_runtime_sanitizers.py
g++ -std=c++20 -O1 -g -fsanitize=undefined -fno-sanitize-recover=undefined -Iinclude tests/test_runner.cpp tests/test_lexer.cpp tests/test_parser.cpp src/lexer.cpp src/parser.cpp src/ast.cpp src/token.cpp src/diagnostic.cpp src/error_registry.cpp -o /tmp/nextviper-front-end-ubsan
/tmp/nextviper-front-end-ubsan
```

Front-end UBSan is scoped to the lexer/parser; it does not instrument the entire
interpreter/VM or prove slice memory safety. The regular differential regression
matrix checks slice results and process status. Native fixtures/security results
are preserved in the artifacts directory. Function return values are tested via
observable calls; there is no separate process return-value protocol.

The missing-device branch was also exercised using the real Vulkan loader with
an intentionally nonexistent driver manifest. This isolates absence reporting;
it is additional validation, not five real GPU executions:

```sh
g++ -std=c++20 -O1 -Iinclude tests/test_runner.cpp tests/test_gpu_subsystem.cpp build-cmake/libnextviper_core.a -lvulkan -lpq -lcurl -pthread -o /tmp/nextviper-gpu-tests
VK_DRIVER_FILES=/tmp/nextviper-no-vulkan-driver.json VK_ICD_FILENAMES=/tmp/nextviper-no-vulkan-driver.json /tmp/nextviper-gpu-tests
```

The [captured result](artifacts/gpu-unavailable.txt) reports 3 passed and 5 skipped.
This overlaps the main suite and is not added to its reconciled totals.

## Reproductions and corrected behavior

- A 30,000-level generic annotation crashed the original compiler with SIGSEGV
  (-11), without stderr. It now exits 1 with a parsing-depth diagnostic.
- Scalar escape regression initially failed (10/11 lexer cases passed); the
  corrected lexer passed 11/11 cases including exact boundary encodings.
- The original two slice methods had three failing backend subtests, including
  `std::bad_alloc` and stale values after bound expressions resized the list.
  The fixed cases and 216-combination matrix pass in interpreter and VM.
- Exact native verification now examines all map/Fibonacci output and stderr.
  Only Fibonacci's numeric elapsed-time value is normalized after format checks.

See [the engineering findings](nextviper-engineering-audit.md) for original file
locations, severity and changes. No Critical vulnerability was confirmed; parser
stack exhaustion and unsafe slice paths were classified High.

## Performance measurements

The [benchmark harness](../../benchmarks/core.py) verifies each workload's output,
uses private temporary files, enforces subprocess deadlines, excludes one warmup,
and records three measured wall-clock samples. Startup time is included in each
script workload. Compilation times measure generation plus host C++ compilation
of a tiny program. Raw samples and exact binary hashes are retained in
[before](artifacts/benchmark-before.json), [after](artifacts/benchmark-after.json)
and [provenance](artifacts/benchmark-provenance.json).

| Workload | Before median ms | After median ms |
| --- | ---: | ---: |
| startup | 125.322 | 72.982 |
| calls | 86.682 | 112.827 |
| loops | 97.089 | 152.250 |
| recursion | 160.893 | 125.623 |
| strings | 133.363 | 61.761 |
| collections | 69.716 | 78.957 |
| allocation | 65.114 | 58.490 |
| compilation | 7772.117 | 20258.220 |
| native_startup | 15.274 | 24.310 |

The baseline binary already included the Unicode/ASCII lexer fix, but preceded
the parser/slice changes. Concurrent tests/compilation and an emulated or managed
runtime caused considerable noise. These are reproducible workloads and actual
samples, **not controlled evidence of a speedup or slowdown**. No optimization
or comparison against Python, Rust or C++ is claimed. No profiler-based bottleneck
analysis was completed. Repeat on an idle pinned host before optimization work.

Reproduction:

```sh
python3 benchmarks/core.py --binary build-cmake/bin/nextviper --label local --output /tmp/nextviper-bench.json --repeat 3
```

## Exact blockers and limits

ASan failed before test code with:
`sanitizer_allocator_primary64.h:131`, `address_range.Init`, allocator address
reservation `(0x500000000000, 0xfffffffffffffff4)`. See the
[captured failure](artifacts/asan-initialization-blocker.txt). It is not a passing
ASan test. Full sanitizer builds were interrupted by runtime syscall failures,
including `Function not implemented`, libuv `uv__io_poll` assertions and compiler
process failures. Focused UBSan succeeded on the resumed environment.

Attempts to install a disposable PostgreSQL server were interrupted by
`Function not implemented` in file operations and then `FATAL -> Failed to fork`.
No server was provisioned; all three live-database tests are honestly skipped.
The detected Vulkan device is llvmpipe, not physical GPU hardware. The final
unit rerun executed its software Vulkan cases; missing-device guards now report
explicit skips. Windows, macOS, Linux x86_64 and physical GPU execution were not
locally validated. Existing CI definitions do not prove their hosted
jobs passed, and hosted CI was not polled in this session.

The Make entry point was updated to include the new examples and grammar checks;
a separate clean Make build was not repeated. CMake performed the actual local
build. Existing release/package test coverage is scoped to distribution cases;
no new release archive was published or production deployment exercised.

## Readiness decision

Still experimental. Recommend maintainer review and `1.0.1-rc.1`, conditional on
hosted CI and target-platform validation. No release was published. Reference
cycles, incomplete type/library validation, VM capture/import limitations,
unsupported native slicing and language exceptions remain technical debt.
Independent adoption evidence and upstream grammar/classifier tests are missing
for GitHub Linguist. The [submission draft](../linguist/submission-draft.md) makes
no recognition claim. Contributor readiness improves through tested small apps,
Hello World, corrected dependencies, architecture, AI disclosure and roadmap.
See [the next five engineering tasks](../roadmap.md).


## Commit boundaries

- `544fced`: verified compiler correctness fixes and regression coverage.
- `42d24fc`: validation tooling, explicit GPU skips, grammar corrections, CI bounds
  and tested practical applications.
- The subsequent documentation commit contains this report, raw evidence,
  contributor guides and Linguist draft. Use `git log bcf1394..HEAD --oneline`
  to identify that commit without embedding a self-referential hash.
