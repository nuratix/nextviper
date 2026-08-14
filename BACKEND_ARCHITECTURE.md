# NextViper Production Backend Architecture & Backend Evaluation

---

## 1. Executive Summary

As NextViper transitions from language specification and baseline execution into its ecosystem and high-performance backend phase, this document establishes the production compiler architecture and evaluates code generation backends (**LLVM** vs **Cranelift**).

NextViper is designed for high-performance computing, automation, AI, and data processing. To fulfill these design requirements without sacrificing developer ergonomics or safety, NextViper utilizes a multi-tier compilation pipeline with a typed Intermediate Representation (IR) lowering into machine code.

---

## 2. Production Compiler Pipeline

The complete compilation and execution pipeline is structured as follows:

```
NextViper Source (.nv)
       ↓
     Lexer (Scanner / Tokenizer)
       ↓
    Parser (Pratt Precedence & AST Construction)
       ↓
   AST (Abstract Syntax Tree with SourceSpan)
       ↓
  Type Checker (Static Inference, Type Propagation & Safety)
       ↓
 NextViper Typed IR (Three-Address Code SSA-Style Control Flow Graph)
       ↓
   IR Optimizer (Constant Folding, Dead-Code Elimination, Mem2Reg)
       ↓
 Native Backend (LLVM Code Generation / Machine Binary Emitter)
       ↓
 Standalone Native Machine Binary (ELF / Mach-O / PE)
```

### Stage Responsibilities

1. **Lexer**: Converts raw UTF-8 source into a strongly-typed token stream with exact column, line, and file path spans for precise Rust-style compiler diagnostics.
2. **Parser**: Constructs a syntax tree using recursive-descent parsing with Pratt expression precedence for operators, block scoping, arrow functions, and pipeline operators (`|>`).
3. **AST**: High-level semantic representation supporting visitor pattern traversal, tree rewriting, and pretty printing.
4. **Type Checker**: Performs bidirectional type inference, validates function parameter signatures, enforces explicit immutability (`let` vs `let mut`), and verifies collection constraints.
5. **NextViper Typed IR**: A machine-independent, linear Three-Address Code (3AC) representation with basic blocks, explicit control flow graphs (CFG), typed virtual registers (`r0`, `r1`, ...), and typed operations.
6. **IR Optimizer**: High-level optimization passes before native code generation:
   - Constant folding and constant propagation.
   - Algebraic simplification and strength reduction.
   - Dead-code elimination (DCE) for unreachable basic blocks and unused variable stores.
   - Tail-call identification and inlining.
7. **Native Backend**: Lowers NextViper Typed IR into target-specific machine instructions with vectorization and link-time optimizations.

---

## 3. Backend Evaluation: LLVM vs Cranelift

To select the production native code generator, LLVM and Cranelift were evaluated across six critical dimensions:

| Evaluation Dimension | LLVM | Cranelift | NextViper Priority | Winner |
| :--- | :--- | :--- | :--- | :--- |
| **1. Runtime Performance** | World-class global optimizations, polyhedral loop transformations, auto-vectorization (AVX-512, NEON, SVE), LTO. | Baseline code generation; lacks aggressive vectorization and polyhedral loop optimizers. | **Critical** (AI, numeric tensors, data workflows) | **LLVM** |
| **2. Compilation Speed** | Heavier compilation pipeline; moderate to slow compile times for deep optimization levels (`-O3`). | Extremely fast compilation designed for real-time JIT compilation (WebAssembly). | **Medium** (AOT builds prioritize peak speed) | **Cranelift** |
| **3. Cross-Platform Support** | Universal tier-1 support (x86_64, aarch64, ARM, Apple Silicon, Windows MSVC, RISC-V, WASM, embedded). | Solid x86_64 and aarch64; experimental Windows MSVC ABI, limited 32-bit/specialized platform support. | **High** (Linux, macOS, Windows, Docker) | **LLVM** |
| **4. Integration Complexity** | Native C++ API (`llvm::IRBuilder`, `llvm::Module`). Directly compatible with NextViper C++20 core without FFI. | Written in Rust. Requires C FFI boundary layer (`cranelift-c` or cdylib), requiring dual toolchains (Rust + C++). | **High** (Maintainability & toolchain simplicity) | **LLVM** |
| **5. Debugging & Tooling** | Industry-standard DWARF5, Windows CodeView/PDB, full GDB, LLDB, and Valgrind interoperability. | Basic DWARF support; limited Windows PDB support. | **High** (Enterprise debugging experience) | **LLVM** |
| **6. Future GPU / AI Support** | Direct integration with MLIR, NVPTX (CUDA), AMDGPU, SPIR-V, WebGPU, and tensor intrinsics (`llvm.matrix`). | No native GPU target backends (targets CPU machine code exclusively). | **Critical** (First-class AI/Tensor roadmap) | **LLVM** |

---

## 4. Architectural Decision & Justification

### Selected Backend: **LLVM**

**Primary Reasons for Selection:**

1. **Raw Numerical & AI Performance**: NextViper's core mission includes AI model inference, dense tensor linear algebra (`tensor.matmul`), and tabular dataset processing. LLVM's automatic vectorizer (SIMD), loop unrolling, and auto-broadcasting deliver the mathematical throughput essential for AI and data science workloads.
2. **Direct C++20 Synergy**: NextViper's compiler is built in Modern C++20. LLVM provides a native C++ API (`llvm::IRBuilder`, `llvm::LLVMContext`, `llvm::Module`), eliminating foreign function interface (FFI) overhead, marshaling bottlenecks, and complex dual-compiler build dependencies.
3. **Hardware & GPU Acceleration Path**: LLVM provides the foundation for **MLIR** (Multi-Level Intermediate Representation) and native GPU compilation backends (`nvptx64-nvidia-cuda`, `amdgcn-amd-amdhsa`, and SPIR-V). This aligns with NextViper's roadmap for hardware acceleration.
4. **Platform & ABI Stability**: LLVM guarantees standard C ABI compliance on all major operating systems (Linux glibc/musl, macOS Darwin, Windows MSVC/MinGW).

### Role of Cranelift
Cranelift remains an attractive candidate for a future **ultra-fast development JIT** mode (e.g. `nextviper dev` or instant REPL execution), but LLVM is chosen as the primary production native AOT compiler backend.

---

## 5. Working Native Compilation Prototype

NextViper includes a fully functional, verified native compilation pipeline:

### Sample Program (`test_sum.nv`):
```nextviper
let x = 10
let y = 20
print(x + y)
```

### Execution Pathways:

1. **Tree-Walk Interpreter**:
   ```bash
   ./bin/nextviper run test_sum.nv
   # Output: 30
   ```

2. **Native AOT Compiler (`nextviper build --native`)**:
   ```bash
   ./bin/nextviper build test_sum.nv -o test_sum_bin --native
   ./test_sum_bin
   # Output: 30
   ```

### Equivalence Verification:
Automated test suite (`tests/test_native_compiler.cpp`) verifies that:
- The tree-walk interpreter produces `30\n`.
- The native compiler generates a standalone machine executable that runs outside the interpreter process and produces `30\n`.
- Both engines produce exact equivalent outputs across arithmetic, variable scopes, expressions, and function calls.

---

## 6. Next Steps

1. **LLVM Direct Bitcode & Object Generation**: Lower NextViper Typed IR directly into LLVM IR modules with target machine object emission.
2. **Runtime ABI Integration**: Connect the native compiler to NextViper's runtime memory management and native tensor math kernels.
3. **Vectorized Loop Transformations**: Implement loop vectorization annotations in the IR optimizer to feed LLVM vector pipelines.
