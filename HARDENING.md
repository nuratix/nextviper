# Hardening work following the readiness review

The independent review reproduced wrong native results, process crashes, shell injection, damaged formatting, lost REPL declarations and incomplete build artifacts. This change addresses those specific cases and reduces unsupported documentation claims. It does not establish production readiness.

## Changes, in order

1. HTTP client execution now uses POSIX argument-vector process spawning and an anonymous capture file. Methods, headers and URL schemes are checked; curl receives connection/total timeouts and an output limit. The CLI search/run-after-compile paths also avoid a shell for arguments.
2. DataFrame wrappers reject incorrect argument types and row widths. Collection rendering detects cycles and bounds nesting. Equality traversal has a nesting guard. Primitive annotated declarations and assignments are checked by the interpreter.
3. REPL submissions are retained until the session ends so callable declarations are not freed after each input.
4. Native code generation uses a standalone tagged C++20 runtime instead of casting numbers to pointers or truncating arguments to integers. Captured variable cells support lexical closures. IR handling now preserves block scope, remainder assignment, power, range steps and short-circuit logic. Unsupported native modules, builtins and slicing are diagnosed.
5. Static checking rejects immutable reassignment and invalid top-level/function control flow. Missing input files produce a failing status and diagnostics.
6. Formatting preserves existing indentation and checks the lexical token stream before returning a rewrite. Arrow, power and remainder-assignment operators remain intact. It does not guess missing indentation or claim to be an AST formatter.
7. The default build produces a native executable. Persistent bytecode output is explicitly unavailable until serialization/loading exists. `run --vm` explicitly selects in-memory VM execution, and the benchmark harness now selects the engines it labels.
8. CMake declares the same implementation/test units as Make, finds PostgreSQL/Vulkan/Threads, builds the language server, registers CTest checks, and offers optional sanitizer instrumentation. CI runs source checks on pushes and pull requests. Future release artifacts are labelled prereleases.
9. Standard-library test helpers reject execution failures and `FAIL_` markers rather than accepting an unconditional success token. Beginner testing docs use assertions. The GPU linear-training test uses fixed starting weights while retaining its convergence threshold; random initialization had made its outcome unstable.
10. README/security/performance/platform status were rewritten. Contradictory audit/readiness/benchmark conclusions were superseded, and old benchmark numbers were withdrawn. Source version 1.0.0 is retained for compatibility, with experimental status clearly documented.

## Reproduce the checks

```sh
make -j2 test
python3 tests/native_verify/run_all.py
python3 tests/test_native_security.py
python3 tests/test_http_hardening.py
```

`tests/test_regressions.py` exercises behavior through the CLI, including native/interpreter equivalence, process exit status, formatting behavior and a fake curl command with harmless injection markers. CMake/CTest and sanitizer commands are in [TESTING.md](TESTING.md).

Current local results and their scope are recorded in [HARDENING_VALIDATION.md](HARDENING_VALIDATION.md).

## Remaining work

Reference cycles can retain memory. Runtime custom/generic annotations, VM equivalence, numeric overflow and allocation/resource handling need broader coverage. Native support remains a subset. Package/archive security and external infrastructure need further review. Untrusted programs need OS isolation; untrusted URLs need application-level SSRF controls. Windows/macOS and production workloads have not been established by Linux tests.
