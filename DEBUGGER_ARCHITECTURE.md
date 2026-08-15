# NextViper Debugger Architecture & Roadmap

This document outlines the technical design, investigation, and roadmap for debugging support in NextViper.

---

## 1. Investigation of Current Runtimes

NextViper provides two primary execution pipelines:
1. **Bytecode Virtual Machine (`src/vm.cpp`)**:
   - Stack-based VM with bytecode instruction pointer (`ip_`), call frames (`CallFrame`), and chunk line info mappings.
   - **Feasibility**: Can support a native bytecode debugger / Debug Adapter Protocol (DAP) by introducing execution step hooks, opcode breakpoints, and stack frame inspection.
2. **Ahead-Of-Time Native Machine Code (`src/native_compiler.cpp`)**:
   - Generates native x86_64 machine code via C++ backend compilation.
   - **Feasibility**: Direct support for GDB / LLDB requires emitting DWARF debug symbols (`.debug_info`, `.debug_line`).

---

## 2. Planned DAP (Debug Adapter Protocol) Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                 IDE / DAP Client (VS Code)                  │
└──────────────────────────────┬──────────────────────────────┘
                               │ DAP Protocol over JSON-RPC
                               ▼
┌─────────────────────────────────────────────────────────────┐
│                 nextviper-dap (Adapter Daemon)              │
│                                                             │
│  - Breakpoint Manager (line & condition)                    │
│  - Variable Scope Inspector                                 │
│  - Stack Frame Unwinder                                     │
│  - Step-Over / Step-Into / Continue Controller              │
└──────────────────────────────┬──────────────────────────────┘
                               │
                               ▼
┌─────────────────────────────────────────────────────────────┐
│          NextViper VM Debug Hook / Native DWARF             │
└─────────────────────────────────────────────────────────────┘
```

---

## 3. Status

Full DAP debugger daemon implementation is tracked under [`PENDING.md`](file:///root/nextviper/PENDING.md) to ensure no mocked or partial debugger features are exposed until complete.
