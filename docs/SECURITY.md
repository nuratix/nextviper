# NextViper Security & Reliability Architecture

## 1. Executive Summary & Threat Model

NextViper is designed from the ground up to provide high-performance scientific, systems, and AI execution while enforcing strict memory safety, resilience against malformed inputs, sandboxed execution bounds, and zero accidental arbitrary code execution during dependency resolution or static analysis.

This document provides the security audit specification, threat model, resource bounding invariants, and fuzz-testing methodology for the NextViper compiler, VM, type checker, runtime, and ecosystem tooling.

```
+-------------------------------------------------------------------------------+
|                             NextViper Defense Model                           |
+-------------------------------------------------------------------------------+
| 1. Input Hardening   | Lexer/Parser recursion guards, UTF-8/byte sanitization  |
| 2. Static Safety     | Strict type checker, manifest JSON isolation           |
| 3. Execution Bounds  | Call stack limit (1000), Array cap (10M), Tensor cap    |
| 4. Memory Safety     | RAII smart pointers, zero dangling pointers, no UAF    |
| 5. Module Sandbox    | Path traversal prevention (canonical path enforcement) |
+-------------------------------------------------------------------------------+
```

---

## 2. Security Review Matrix

### 2.1 Memory Safety
- **Ownership & Lifetime Model**: NextViper AST nodes, bytecode chunks, and values utilize modern C++20 RAII idioms with `std::unique_ptr` for single-ownership AST hierarchies and `std::shared_ptr` for runtime reference-counted environments, strings, arrays, objects, and function closures.
- **Dangling Pointer Elimination**: AST visitor references and function declaration references are bound to lifetimes explicitly managed in `Program` and `ModuleManager` storage pools.
- **Zero Raw Pointers in Public APIs**: All tensor data buffers use custom `std::shared_ptr<void>` deleter handles tied to `CPUTensorBackend` memory pools.

### 2.2 File Access & Path Traversal Prevention
- **Canonical Path Resolution**: The NextViper module resolution engine uses `std::filesystem::canonical` and resolves strictly relative to the importing file or registered system package roots.
- **Directory Traversal Defense**: Relative imports (`./`, `../`) are sanitized and validated to prevent unauthorized directory traversal escape beyond intended project roots.
- **Safe Reading Defaults**: File processing standard libraries (`data.from_csv`, `sys.read_file`) enforce maximum file size thresholds to prevent heap exhaustion.

### 2.3 Process Execution & Dependency Handling
- **Non-Executable Package Manifests**: `nextviper.json` package configuration uses declarative JSON parsing without dynamic script evaluation.
- **Deterministic Resolution**: Package resolution strictly parses version constraints and file structures without running pre-install lifecycle scripts or native compilation hooks during resolution.
- **Isolated Module Environments**: Each imported module executes within its own top-level `Environment` scope. Module symbols are exported explicitly via `export` statements; non-exported module globals remain private to the module namespace.

### 2.4 Resource Limits & Denial-of-Service (DoS) Protections

| Subsystem | Guard / Invariant | Limit / Threshold | Mitigation Behavior |
| :--- | :--- | :--- | :--- |
| **Parser Nesting** | `MAX_RECURSION_DEPTH` | `500 frames` | Emits `error[NV110]: maximum parsing nesting depth exceeded`, halts AST recursion cleanly |
| **Call Stack Depth** | `MAX_CALL_STACK_DEPTH` | `1000 frames` | Emits `RuntimeError: maximum call stack depth exceeded`, prevents C++ host stack overflow |
| **Array Allocations** | `MAX_ARRAY_ELEMENTS` | `10,000,000 items` | Validates `range()`, `push()`, `append()` bounds; raises `RuntimeError` before heap exhaustion |
| **String Allocation** | `MAX_STRING_BYTES` | `64 MB` | Limits repeated string concatenations and large string multiplications |
| **Tensor Numel** | `MAX_TENSOR_NUMEL` | `100,000,000 elements` | Performs checked multiplication (`dim > MAX / numel`) preventing integer overflow wrapping |
| **Parser Error Storm** | `MAX_DIAGNOSTIC_ERRORS` | `100 errors` | Halts cascading parse loop on chaotic token streams to prevent parsing runaway |

---

## 3. Fuzz Testing Suite & Methodologies

NextViper includes an automated, multi-tiered fuzzing harness located in `tests/test_fuzz.cpp` verifying that the compiler and runtime never crash, segfault, or hang on malformed or malicious inputs.

### 3.1 Lexer Fuzzing
- Unterminated string literals and unclosed multiline comments.
- Raw null bytes `\0`, arbitrary non-ASCII binary blobs (`0xFF`, `0xFE`, `0x80`), invalid Unicode escape sequences (`\u999999`).
- Overflowing floating-point exponents (`1e+9999999999999999999999999999999999999999`) and invalid base numbers (`0b10201`, `0xGHIJK`, `0o89`).

### 3.2 Parser Fuzzing
- Deeply nested expressions: `((((... 600 levels ...))))`.
- Dangling delimiters, missing operands (`1 + + + 2`), unclosed blocks, and unclosed arrays/maps.
- Null pointer resilience across all expression and statement visitors.

### 3.3 TypeChecker Fuzzing
- Cyclic type aliases, mismatched function parameter/return types, non-callable invocations (`let x = 10; x()`), and undefined identifier lookups.

### 3.4 Interpreter & VM Fuzzing
- Unbounded mutual recursion loops (`fn f(): f(); f()`).
- Division and modulo by zero (`1 / 0`, `1 % 0`).
- Oversized array range requests (`range(0, 1000000000)`).
- Pseudo-random byte stream mutations generated from valid NextViper seeds using pseudo-random byte substitution, insertion, and deletion engines.

---

## 4. Security Guidelines for NextViper Developers

1. **Always use `SourceSpan`**: Every AST node, runtime token, and diagnostic must include accurate span data for clear error attribution.
2. **Never invoke `system()` on untrusted input**: All subprocess and CLI operations must sanitize path arguments.
3. **Prefer Checked Arithmetic for Allocations**: When computing memory buffers for tensors or matrices, verify dimension products against `MAX_TENSOR_NUMEL` before calling allocators.
4. **Enforce Static Checking**: Run `nextviper check <file.nv> --format=json` in CI/CD pipelines to catch type mismatches before native compilation.
