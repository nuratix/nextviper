# NextViper Multi-Platform Support Matrix

This matrix accurately documents the operational and build support status of the NextViper toolchain across target operating systems and CPU architectures.

---

## 1. Platform Support Classification

- **`VERIFIED`**: Proven operational via reproducible test execution on native hardware/runners in the current environment.
- **`BUILDABLE`**: Clean source code compatibility with standard C++20 toolchains; pending automated CI hardware runners.
- **`UNVERIFIED`**: Architecture designed and implemented in source, but untested on physical hardware.
- **`NOT_SUPPORTED`** / **`NOT_PLANNED`**: Explicitly out of scope or unsupported.

---

## 2. Platform Support Table

| Operating System | Architecture | Toolchain / Compiler | Support Status | Notes |
| :--- | :--- | :--- | :--- | :--- |
| **Linux** | `x86_64` (amd64) | GCC 10+, Clang 12+ | `VERIFIED` | Full interpreter, bytecode VM, native AOT compiler, Vulkan GPU backend, and stdlib. |
| **Linux** | `aarch64` (arm64) | GCC 10+, Clang 12+ | `VERIFIED` | Full interpreter, VM, native AOT, and Vulkan GPU backend. |
| **macOS** | `arm64` (Apple Silicon M1/M2/M3) | Apple Clang (Xcode 13+) | `BUILDABLE` | Source C++20 compatible with POSIX runtime; prebuilt binary artifacts pending Darwin CI runner. |
| **macOS** | `x86_64` (Intel) | Apple Clang (Xcode 13+) | `BUILDABLE` | Source C++20 compatible with POSIX runtime; prebuilt binary artifacts pending Darwin CI runner. |
| **Windows** | `x86_64` (amd64) | MSVC 2019+ / MinGW-w64 | `BUILDABLE` | CMake build configuration provided; automated prebuilt Windows binary artifacts pending CI runner. |
| **Android** | `aarch64` / `armv7` | Clang + Make in Termux | `BUILDABLE` | Manual source build supported. Official `pkg install` is `NOT_PLANNED`. |

---

## 3. Acceleration & Subsystem Capabilities by Platform

| Capability | Linux (x86_64 / arm64) | macOS (Apple Silicon / Intel) | Windows (x86_64) |
| :--- | :--- | :--- | :--- |
| **Interpreter & Bytecode VM** | `VERIFIED` | `BUILDABLE` | `BUILDABLE` |
| **Native AOT Compiler** | `VERIFIED` | `BUILDABLE` | `BUILDABLE` |
| **Tensor Engine & Autograd** | `VERIFIED` | `BUILDABLE` | `BUILDABLE` |
| **Vulkan GPU Compute** | `VERIFIED` | `BUILDABLE` (MoltenVK) | `BUILDABLE` (Vulkan SDK) |
| **Standard Library Sockets & FS** | `VERIFIED` | `BUILDABLE` | `BUILDABLE` |
| **Language Server Protocol (LSP)** | `VERIFIED` | `BUILDABLE` | `BUILDABLE` |
