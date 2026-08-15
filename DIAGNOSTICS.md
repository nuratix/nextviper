# NextViper Compiler Diagnostics System

NextViper features an exact-location compiler diagnostics engine designed for fast developer feedback both on the command line and in IDEs.

---

## 1. Diagnostic Architecture

The compiler's diagnostic engine (`DiagnosticEngine`) tracks:
- **Exact Line & Column Spans**: `SourceSpan` records start and end line/column coordinates.
- **Diagnostic Severity**:
  - `ERROR`: Halts compilation; prevents invalid execution.
  - `WARNING`: Highlights suspicious patterns, unused variables, or performance hazards.
  - `NOTE` / `HELP`: Contextual hints with actionable suggestions.
- **Unique Error Codes**: Standardized error codes (e.g. `NV100`, `NV102`, `NV114`) for documentation lookup.

---

## 2. Formats

### Terminal Output (ANSI Colored)
```
error[NV102]: undefined variable 'foo'
  --> src/main.nv:12:9
   |
12 |     let x = foo + 1
   |             ^^^ help: did you mean 'food'?
```

### JSON Output (`nextviper check --format=json`)
```json
[
  {
    "code": "NV102",
    "level": "error",
    "message": "undefined variable 'foo'",
    "span": {
      "file": "src/main.nv",
      "start_line": 12,
      "start_column": 9,
      "end_line": 12,
      "end_column": 12
    },
    "hint": "did you mean 'food'?"
  }
]
```

### LSP Integration (`nextviper-lsp`)
Automatically mapped to LSP `textDocument/publishDiagnostics` with 0-indexed positions for native editor integration.
