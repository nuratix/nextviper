# NextViper Developer Tooling Architecture

NextViper provides a unified, zero-overhead developer tooling ecosystem built on top of the native C++20 compiler, lexer, recursive descent parser, and static type system.

---

## 1. Design Philosophy

- **Zero Duplication**: The Language Server Protocol (`nextviper-lsp`), CLI commands (`check`, `fmt`, `test`, `info`), and VS Code extension reuse the exact same compiler AST and parser without maintaining separate parsing grammars.
- **Strict Realism & Zero Mocks**: Every completion item, hover type tooltip, diagnostic squiggle, definition jump, and symbol outline comes directly from live AST traversal and standard library definitions.
- **Fast Interactive Response**: Parsing and analysis are structured for sub-millisecond document indexing and immediate JSON-RPC response latency.

---

## 2. Tooling Architecture Diagram

```
┌─────────────────────────────────────────────────────────────┐
│                 IDE / Client Interface                       │
│    (VS Code Extension / Terminal CLI / External LSP Clients) │
└──────────────────────────────┬──────────────────────────────┘
                               │ JSON-RPC 2.0 / CLI Subcommands
                               ▼
┌─────────────────────────────────────────────────────────────┐
│             NextViper Developer Tooling Core                │
│                                                             │
│  ┌────────────────────┐   ┌──────────────────────────────┐  │
│  │   nextviper-lsp    │   │      nextviper CLI Tool      │  │
│  │  (Language Server) │   │ (fmt, check, test, repl, ..) │  │
│  └─────────┬──────────┘   └──────────────┬───────────────┘  │
└────────────┼─────────────────────────────┼──────────────────┘
             │                             │
             ▼                             ▼
┌─────────────────────────────────────────────────────────────┐
│              NextViper Native Compiler Core                 │
│                                                             │
│   Lexer ──► Recursive Parser ──► AST ──► Type Checker       │
│                                   │                         │
│                    ┌──────────────┴──────────────┐          │
│                    ▼                             ▼          │
│             Bytecode VM                   Native AOT        │
│          (High-speed Eval)            (Optimized Binary)    │
└─────────────────────────────────────────────────────────────┘
```

---

## 3. Subsystem Breakdown

### 1. Language Server Protocol (`src/lsp.cpp`, `include/nextviper/lsp.hpp`)
- Implements standard LSP JSON-RPC 2.0 over `stdin`/`stdout`.
- Supported Capabilities:
  - `initialize`, `shutdown`, `exit`
  - `textDocument/didOpen`, `textDocument/didChange`, `textDocument/didClose`
  - `textDocument/publishDiagnostics` (Mapped from `DiagnosticEngine`)
  - `textDocument/completion` (Keywords, stdlib, modules, local variables/functions)
  - `textDocument/hover` (Signatures, markdown docstrings, type annotations)
  - `textDocument/definition` (Zero-latency jump to symbol declaration)
  - `textDocument/references` (Find usages across AST)
  - `textDocument/documentSymbol` (Outline view tree)
  - `workspace/symbol` (Workspace-wide symbol search)
  - `textDocument/formatting` (Invokes deterministic AST formatter)

### 2. Static Checker (`nextviper check`)
- Performs multi-stage verification without emitting machine code:
  1. Lexical and syntax validation.
  2. AST construction and scope validation.
  3. Static type inference and assignment consistency.
- Supports `--format=json` for automated CI/CD gating and tool integration.

### 3. Automated Test Runner (`nextviper test`)
- Automatically discovers and executes all `.nv` test suites in `tests/`.
- Measures per-test latency and outputs colorized summaries with pass/fail counts.
- Runs compiler unit test harness when invoked in compiler repository.

### 4. Deterministic Formatter (`nextviper fmt`)
- Idempotent and AST/token-driven.
- Normalizes operator spacing, consistent 4-space block indentation, and preserves line comments.
- Supports `nextviper fmt --check` returning non-zero exit codes for CI formatting validation.

### 5. Interactive REPL (`nextviper repl`)
- Provides an immediate evaluation shell with variable persistence, multi-line blocks, and rich error reporting.

### 6. VS Code Extension (`editors/vscode/`)
- Native TypeScript Language Client bridging VS Code with `nextviper-lsp`.
- Bundles full TextMate syntax grammar (`syntaxes/nextviper.tmLanguage.json`).
