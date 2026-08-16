# NextViper Build System Architecture

The NextViper build system provides unified compilation across interpreted, bytecode, and native machine code execution targets.

---

## 1. Build Pipeline

The NextViper compilation pipeline consists of 7 deterministic stages:

```
Source Code (.nv)
       │
       ▼
1. Lexer (Tokens & Span Metadata)
       │
       ▼
2. Parser (Abstract Syntax Tree)
       │
       ▼
3. Type Checker (Static Type Validation)
       │
       ▼
4. Dependency Resolver (nextviper.toml & nextviper.lock)
       │
       ▼
5. Intermediate Representation (IR / SSA Lowering)
       │
       ▼
6. IR Optimizer (Constant Folding & Dead Code Elimination)
       │
       ▼
7. Native Code Generator / VM Bytecode Emitter
       │
       ▼
Executable Binary (`build/bin/<target>`) or Bytecode Package (`.nvc`)
```

---

## 2. Compilation Targets

### A. Native Machine Code (`--native`)
- **Backend:** C11 / C++20 standard compiler (`g++` / `clang++`).
- **Performance:** Native CPU execution with zero virtual machine overhead.
- **Output:** Standalone ELF executable on Linux, Mach-O on macOS, PE on Windows.
- **Flags:**
  - `--release`: Compiles with `-O3 -fomit-frame-pointer` for maximum throughput.
  - `--debug`: Compiles with `-g -O0` for full source-level debugging with GDB or LLDB.

### B. Bytecode Virtual Machine (`--bytecode`)
- **Format:** Portable `NVBC` chunk file containing serialized opcodes, constant pool, and debug line tables.
- **Use Case:** Fast distribution, sandboxed execution, and scripting workflows.

---

## 3. Project Manifest (`nextviper.toml`)

When invoked in a project workspace, `nextviper build` automatically reads `nextviper.toml`:

```toml
[project]
name = "api_service"
version = "1.0.0"
description = "High-performance microservice"
license = "MIT"
main = "src/main.nv"

[dependencies]
postgres_driver = "^1.2.0"
jwt_auth = "^0.4.1"
```

The compiled binary is written to `build/bin/api_service`.

---

## 4. Artifact Management

- `build/`: Contains compiled object files, intermediate C code, and final binaries.
- `build/bin/`: Contains the generated standalone executables.
- `nextviper clean`: Safely purges the `build/` directory without altering source code.
