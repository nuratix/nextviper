# NextViper Native Platform Support & Architecture

## Compilation Architecture

NextViper's Native AOT Compiler uses a high-performance, architecture-neutral translation pipeline:

```
[NextViper Source Code (*.nv)]
            │
            ▼ (Lexer & Parser)
[AST (Abstract Syntax Tree)]
            │
            ▼ (IR Generator & Optimizer)
[NextViper SSA Typed IR (Three-Address Code)]
            │
            ▼ (C Source Emitter)
[Optimized Generated C Source (*.tmp.c)]
            │
            ▼ (Direct POSIX Process Execution: posix_spawnp)
[Host System C Compiler (GCC / Clang -O3)]
            │
            ▼ (Native Linker)
[Standalone Machine Binary Executable (ELF / Mach-O / PE)]
```

### Key Architectural Characteristics
1. **Portable Optimization**: NextViper leverages the host system's C compiler (GCC, Clang) with `-O3` to perform architecture-specific instruction scheduling, auto-vectorization (SIMD), register allocation, and native ABI calling conventions.
2. **Safe Process Spawning**: Compilation commands are executed via direct `posix_spawnp` argument vectors rather than shell string interpretation (`sh -c` / `std::system`), eliminating shell injection vulnerabilities when paths contain spaces or metacharacters.
3. **No Direct JIT Machine-Code Emission**: NextViper currently emits optimized C rather than direct x86-64/ARM machine-code bytes. Direct binary backend integration (LLVM/Cranelift) is planned for future major releases.

---

## Supported Architectures & Environments

| Architecture | Platform | Compiler Toolchain | Verification Status |
| :--- | :--- | :--- | :--- |
| **aarch64 (ARM64)** | Linux (Ubuntu, Debian, Alpine) | GCC 10+, Clang 12+ | `SOURCE VERIFIED` |
| **x86_64 (AMD64)** | Linux (Ubuntu, Debian, Fedora) | GCC 10+, Clang 12+ | `SOURCE VERIFIED` |
| **arm64 / x86_64** | macOS 12+ | Xcode Command Line Tools / Clang | `IMPLEMENTED` |
| **x86_64** | Windows 10/11 | MSVC (Visual Studio) / MinGW-w64 | `IMPLEMENTED` |
| **aarch64 / arm** | Android (Termux) | Clang in Termux environment | `SOURCE VERIFIED` |

---

## Verification Test Suite
The native compiler is verified via:
- `tests/test_native_compiler.cpp`: Verifies arithmetic, functions, loops, arrays, and IR constant folding.
- `tests/native_verify/`: Comprehensive 12-file test suite verifying exact interpreter vs native output equivalence.
- `tests/test_native_security.py`: Verifies process execution resilience against spaces and shell metacharacters.
