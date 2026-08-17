# NextViper Native Platform Support Matrix

## 1. Supported Platform Matrix

| Platform / Architecture | Verification Status | C/C++ Toolchain | Native Libraries | Notes |
| :--- | :--- | :--- | :--- | :--- |
| **Linux (x86_64)** | **VERIFIED** | `g++ >= 11` or `clang++ >= 14`, `gcc` (AOT) | `glibc`, `libpthread`, `libm`, `libvulkan`, `libpq` | Primary tier-1 development & test platform. Full test suite passing (154/154). |
| **Linux (aarch64 / ARM64)** | **EXPERIMENTAL** | `g++ >= 11`, `gcc` (AOT) | `glibc`, `libpthread`, `libm` | IR and C code generation are architecture-neutral; requires build toolchain on host. |
| **macOS (Apple Silicon / Intel)** | **EXPERIMENTAL** | Apple Clang / LLVM (`clang++`) | `libc++`, `libpthread`, `MoltenVK`, `libpq` | POSIX runtime compatible; Vulkan requires MoltenVK translation layer. |
| **Windows (x86_64)** | **EXPERIMENTAL** | MSVC 2022 (`cl.exe`) or MinGW-w64 (`g++`) | `ucrt`, `ws2_32`, `vulkan-1.dll`, `libpq.dll` | Win32 socket translation implemented in `module.cpp`; requires Windows SDK. |
| **Android / Termux** | **EXPERIMENTAL** | Clang via Termux packages | `bionic`, `libpthread` | Limited by Android 12+ Phantom Process Killer (Signal 9 SIGKILL) on multi-core builds. |

---

## 2. Compiler Toolchain & Runtime Dependencies

### Native AOT Compiler Pipeline
The NextViper native compiler generates standard ISO C code conforming to C99 / C11 standards and invokes the host C compiler (`gcc` or `clang`) with `-O3` optimization flags:

```
[NextViper Source (.nv)]
         ↓ (Lexer & Parser)
[AST (Abstract Syntax Tree)]
         ↓ (IRGenerator)
[Typed Register IR (SSA-style)]
         ↓ (IROptimizer: Constant Folding & DCE)
[Optimized Typed IR]
         ↓ (NativeCompiler: C Code Emitter)
[Self-Contained C Source (.c)]
         ↓ (Host C Compiler: gcc / clang -O3)
[Standalone Machine Executable]
```

### Compiler Dependencies
- **C Compiler**: `gcc` or `clang` available on the system `$PATH`.
- **Standard C Runtime**: `libc` (POSIX `glibc`, `musl`, or Windows `ucrt`).
- **Standard Math**: `libm` (`sqrt`, `pow`, `cbrt`, `sin`, `cos`, `tan`, `floor`, `ceil`).
- **Timing Primitives**: `clock_gettime` (`CLOCK_MONOTONIC`), `gettimeofday`, `usleep`.
- **Memory Allocator**: `malloc`, `realloc`, `free` for dynamic `NVArray` and `NVMap` heap storage.

---

## 3. Platform Verification Procedure

To verify native compilation on any target platform:
```bash
# 1. Compile test program
nextviper build --native tests/native_verify/01_hello.nv -o /tmp/hello_native

# 2. Execute binary directly
/tmp/hello_native
# Expected output: Hello, World!

# 3. Verify exact equivalence against interpreter
nextviper run tests/native_verify/01_hello.nv
```
