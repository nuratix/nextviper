# NextViper Pending Features & Roadmap

This document explicitly tracks language and tooling capabilities that are scheduled for future development phases, ensuring zero incomplete or mocked features are deployed in production.

---

## 1. Tooling Subsystem

| Feature | Category | Planned Approach |
| :--- | :--- | :--- |
| **Debug Adapter Protocol (DAP)** | Tooling / Debugger | Implement `nextviper-dap` daemon hooking into the Bytecode VM call frames and DWARF emission for native targets. See [`DEBUGGER_ARCHITECTURE.md`](file:///root/nextviper/DEBUGGER_ARCHITECTURE.md). |
| **Semantic Token Highlighting in LSP** | LSP | Full semantic token classification stream (`textDocument/semanticTokens/full`) for high-precision syntax coloring. |
| **Code Actions & Refactoring** | LSP | Quick-fix diagnostic suggestions and automated extract-function refactorings. |
| **Language Tour Interactive Notebooks** | Tooling | WebAssembly / CLI-based interactive notebook playground. |

---

## 2. Compiler & Subsystems

| Feature | Category | Status |
| :--- | :--- | :--- |
| **LLVM IR Backend** | Native Backend | Investigating direct LLVM C++ API codegen as alternative to GCC/Clang C++ bridge. |
| **Distributed Multi-GPU Tensors** | GPU / Tensor | Multi-node GPU tensor communication (NCCL equivalent). |
