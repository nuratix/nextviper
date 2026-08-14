# NextViper Roadmap

This document outlines the strategic engineering roadmap for NextViper across development milestones.

```mermaid
gantt
    title NextViper Engineering Roadmap
    dateFormat  YYYY-Q#
    section Core Evolution
    v0.1.0 Foundation & CLI        :done,    des1, 2026-Q1, 2026-Q1
    v0.2.0 Bytecode VM & IR        :active,  des2, 2026-Q2, 2026-Q2
    v0.3.0 Type Inference & Structs:         des3, 2026-Q3, 2026-Q3
    v0.4.0 AI / Tensor Engine      :         des4, 2026-Q4, 2026-Q4
    v0.5.0 JIT & C Interop (FFI)   :         des5, 2027-Q1, 2027-Q1
    v1.0.0 LLVM AOT Compiler (Native):       des6, 2027-Q2, 2027-Q3
```

---

## Milestone Breakdown

### Phase 1: Foundation & Tooling (Current: v0.1.0)
- [x] Modern C++20 compiler core architecture
- [x] Complete Lexer with location tracking, comments, and modern literals
- [x] Recursive descent AST parser with operator precedence & pipeline syntax (`|>`)
- [x] AST tree structures, AST visitor pattern, and AST printer
- [x] Tree-walk interpreter execution engine with dynamic scoping and mutability enforcement
- [x] Comprehensive standard built-ins (`print`, `len`, `typeof`, `range`, `push`, `pop`, `assert`, math primitives)
- [x] Interactive REPL with command system (`:help`, `:env`, `:version`, `:clear`)
- [x] CLI binary `nextviper` with subcommands (`run`, `eval`, `check`, `parse`, `tokens`, `repl`)
- [x] Built-in unit test framework and end-to-end CLI integration test suite

---

### Phase 2: Bytecode Compiler & Virtual Machine (v0.2.0)
- [ ] **Bytecode Instruction Set**: Design stack-based or register-based bytecode (OpCodes for arithmetic, jump tables, function calls, closures)
- [ ] **Bytecode Compiler**: Transform AST into flat chunk bytecode
- [ ] **Direct-Threaded VM Dispatch**: High-performance virtual machine interpreter loop with instruction cache locality
- [ ] **Memory Management**: Arena allocator and precise mark-and-sweep garbage collector for heap objects
- [ ] **Performance Benchmarks**: Microbenchmarks against Python 3.12, Lua 5.4, and JavaScript V8

---

### Phase 3: Gradual Typing & Data Structures (v0.3.0)
- [ ] **Static Type Checker**: Hindley-Milner type inference engine for local variable definitions
- [ ] **Structs & Value Types**: User-defined types (`struct Point { x: Float, y: Float }`)
- [ ] **Pattern Matching**: `match` expression with exhaustiveness verification and destructuring
- [ ] **Generics**: Generic collections `List[T]`, `Map[K, V]`, `Result[T, E]`, `Option[T]`
- [ ] **Module & Import System**: Modular compilation (`import math`, `from "utils.nv" import logger`)

---

### Phase 4: AI Primitives & Tensor Processing (v0.4.0)
- [ ] **First-Class Tensor Type**: Multi-dimensional contiguous n-arrays with broadcasting
- [ ] **BLAS / LAPACK & SIMD Acceleration**: AVX2/AVX-512 and ARM NEON vectorized math kernels
- [ ] **Autodiff & Computational Graphs**: Reverse-mode automatic differentiation primitives
- [ ] **Data Pipeline Engine**: Lazy evaluation pipelines for streaming datasets and CSV/Parquet ingestion

---

### Phase 5: High-Speed JIT & C Foreign Function Interface (v0.5.0)
- [ ] **Foreign Function Interface (FFI)**: Zero-copy binding with C/C++ libraries and CUDA runtimes
- [ ] **Tier-1 JIT Compiler**: Template JIT / Dynasm / Cranelift backend for hot loop optimization
- [ ] **Native Concurrency**: Async/await runtime with work-stealing thread pools (M:N scheduler)

---

### Phase 6: Production LLVM Native AOT Compiler (v1.0.0)
- [ ] **LLVM Code Generation**: Full AOT compiler generating standalone ELF, Mach-O, and PE executables
- [ ] **Link-Time Optimization (LTO)**: Interprocedural optimization and dead-code stripping
- [ ] **Package Manager (`nvpm`)**: Dependency resolution, package registry, and build manager
- [ ] **Language Server Protocol (LSP)**: Autocomplete, go-to-definition, and real-time diagnostics for VS Code and Neovim
