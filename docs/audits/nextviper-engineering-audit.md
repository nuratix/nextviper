# NextViper engineering audit

Date: 2026-10-31. Starting revision: `bcf1394`.
AI-assisted engineering work; maintainer review is still required.

## Executive assessment and scope

NextViper remains an experimental language. This session cloned the actual
repository, inspected build/test entry points and core execution paths, reproduced
failures, and implemented targeted fixes without replacing the architecture.
This is a bounded engineering audit, not a complete line-by-line review of every
subsystem or a production/security certification. No Critical finding was
confirmed; the High findings below include process crashes and unsafe indexing.

Reviewed directly: lexer, parser/type-annotation recursion, AST accessors,
interpreter/VM slicing, environments, bytecode limits, native code emission and
runtime, native security/equivalence tests, build configuration, workflows,
contributor/status/specification documentation and editor grammar. Existing suites
exercise modules, stdlib, CLI/REPL/tooling, collections, HTTP/transports, AI/data,
package management and native execution. Test execution is not an exhaustive
source audit of these areas. Physical GPU hardware, package/archive trust, live database
coverage and untested platforms need further independent review. Vulkan tests
ran locally using the llvmpipe software device.

## Confirmed findings

Line references below identify the original `bcf1394` source, unless a replacement
file is explicitly named. Use `git show bcf1394:path` to inspect original lines.

| ID | Severity | Original location | Reproduction and consequence | Implemented correction |
| --- | --- | --- | --- | --- |
| NV-A01 | High | `src/parser.cpp:147` | A type annotation containing 30,000 nested `list[` forms crashes `check` with SIGSEGV (exit -11), without a diagnostic | Apply the existing recursion guard to type annotations; CLI regression expects status 1 and a depth diagnostic |
| NV-A02 | High | `src/interpreter.cpp:1049`, `src/vm.cpp:821` | `[0,1,2,3][1:4:9223372036854775807]` can overflow signed loop advancement and index outside storage | Shared bounded advancement in `include/nextviper/slicing.hpp`; extreme strides and matrix tests |
| NV-A03 | High | `src/interpreter.cpp:1053`, `src/vm.cpp:825` | `[0,1,2,3][4:0:-1]` permits `start == length` before reverse indexing; baseline combined boundary test fails with `std::bad_alloc` | Direction-aware clamping; omitted reverse bounds; list/string/empty tests |
| NV-A04 | High | `src/interpreter.cpp:1014` | A bound function that pops two elements from `[1,2,3]` leaves stale length; baseline slice prints removed elements `[1, 2, 3]` | Compute length after all bounds evaluate; regression expects `[1]` |
| NV-A05 | Medium | `src/lexer.cpp:438`, `src/lexer.cpp:454` | `print("\u{110000}")` silently loses the escape; surrogate escapes encode invalid UTF-8 | Reject non-scalar escapes; exact UTF-8 boundary tests |
| NV-A06 | Medium | `src/lexer.cpp:162` and other cctype calls | Passing negative signed chars to cctype violates its input precondition; locale-dependent identifiers also contradict the ASCII specification | ASCII predicates; all bytes 128–255 tested. This portability defect was identified by inspection, not a demonstrated local sanitizer crash |
| NV-A07 | Medium | `tests/native_verify/run_all.py:52` | Partial Fibonacci/map comparisons can accept incorrect unexamined output; stderr is ignored and success count is hardcoded | Compare complete stdout/stderr/status for 13 fixtures; normalize only the measured timing number |
| NV-A08 | Medium | `tests/test_gpu_subsystem.cpp:29` and four other guards | Missing GPU returns normally and is counted as PASS | Explicit skip exception, reason and count in C++ runner |
| NV-A09 | Medium | `editors/vscode/syntaxes/nextviper.tmLanguage.json:44` | Grammar omits `elif`, `loop` and logical keywords while highlighting unsupported exception/class words; short operators split compound tokens | Match lexer keywords and longest operators; metadata/pattern regression tests |
| NV-A10 | Medium | `tests/test_native_security.py:12`, `tests/native_verify/run_all.py:28` | Fixed shared temporary paths can collide; security script hardcodes one checkout and removes fixed paths | Private temporary directories, executable override, bounded subprocesses and exact output checks |
| NV-A11 | Low | `CONTRIBUTING.md:23` | Vulkan is described as optional despite required CMake/Make linkage; database/curl prerequisites are omitted | Correct prerequisites and conservative build parallelism |

The pre-fix Unicode scalar test failed (10/11 lexer tests passed); after the
correction 11/11 passed. The annotation depth test failed before its guard.
The pre-fix combined slice regressions had three failing backend subtests; the
fixed matrix covers 216 bound/stride combinations in each interpreter and VM.
See [validation](nextviper-validation.md) for completed runs and blockers.

## Reproducing the important regressions

```sh
# Generated depth input, then nextviper check deep.nv:
python3 -c 'print("let value: " + "list[" * 30000 + "int" + "]" * 30000 + " = []")' > deep.nv
nextviper check deep.nv
```

```nextviper
let a = [0, 1, 2, 3]
print(a[::-1])
print(a[4:0:-1])
print(a[1:4:9223372036854775807])
```

Expected fixed output is `[3, 2, 1, 0]`, `[3, 2, 1]`, `[1]` on separate lines.
Use `run --tree` and `run --vm`; native slicing remains unsupported.

```nextviper
let a = [1, 2, 3]
fn shrink():
    pop(a)
    pop(a)
    return 0
print(a[shrink():])
```

Expected fixed output: `[1]`. The original interpreter used the stale size.
For invalid Unicode, place a literal backslash-u escape in a `.nv` string;
`\uD800`, `\u{DFFF}` and `\u{110000}` must produce lexical errors.

## Cross-backend contract and unresolved differences

The existing language contract suite covers arithmetic, integer boundaries,
compound assignment/evaluation order, function return values via printed calls,
loops/control-flow, collections and selected error behavior. Existing native
regressions cover closures/recursion, while VM captures/imports are explicitly
rejected. Local module initialization failures are tested in the interpreter.
The three practical apps use supported interpreter modules, with real HTTP I/O.

Native slicing is unsupported. VM imports/captured outer locals are unsupported.
Explicit nil slice bounds remain different between interpreter and VM; omission
is the tested portable contract. Persistent bytecode serialization/loading and
catchable language exceptions are unimplemented. Diagnostic rendering differs
between backends, so error tests compare status and relevant content rather than
claiming byte-identical errors. No separate process return-value protocol exists.

## Security, ownership and maintainability

The fixed parser recursion and slice indexing paths reduce crash risk; they do
not sandbox programs. Language programs retain process filesystem, network and
process privileges. Shared-pointer collection cycles and function/environment
cycles remain known ownership debt; full leak-free execution is not established.
ASan allocator initialization is blocked locally; see the exact validation note.

Native compiler path checks exercise spaces, semicolons and command substitution
through argv. The tests use local markers and private directories. No secrets
are included in source or audit artifacts. The credential shared in the request
must be revoked and replaced; it is intentionally absent from these documents.

Repeated interpreter/VM slicing code was consolidated to avoid divergent fixes.
The audit cannot determine whether particular existing code was AI-generated;
it identifies observable defects and unsupported claims without guessing origin.
Legacy backend evaluation documents and broad GPU/AI/performance claims need
continued reconciliation with CURRENT_STATUS.md and measured evidence.

## Release and contributor readiness

README now starts with a literal Hello World; architecture includes a pipeline
diagram; contributor prerequisites, AI disclosure, changelog, troubleshooting,
practical applications and a prioritized roadmap were added or corrected.
Existing conduct/security policies were retained. These improvements make the
project easier to evaluate, not production-ready. Recommend `1.0.1-rc.1` for
maintainer consideration after CI/review; the version string was not changed and
no release was published. GitHub Linguist eligibility is not demonstrated; the
local metadata/submission draft explicitly lists missing adoption and upstream
validation. See [the next five tasks](../roadmap.md).
