# NextViper Language Server Protocol (LSP) Specification

The NextViper Language Server (`nextviper-lsp`) implements the official Microsoft Language Server Protocol specification version 3.17 to integrate NextViper language intelligence into any modern editor or IDE.

---

## 1. Running the Language Server

The LSP daemon can be executed directly as a standalone binary or via the `nextviper` CLI:

```bash
# Standalone binary
nextviper-lsp

# Via CLI subcommand
nextviper lsp
```

Communication occurs over standard input/output (`stdin`/`stdout`) using HTTP-style `Content-Length` framing and JSON-RPC 2.0.

---

## 2. Supported LSP Methods

### Lifecycle
| Method | Description |
| :--- | :--- |
| `initialize` | Client handshake. Server responds with supported capabilities. |
| `initialized` | Notification confirming client readiness. |
| `shutdown` | Requests graceful daemon shutdown. |
| `exit` | Exits the server process. |

### Document Synchronization
| Method | Description |
| :--- | :--- |
| `textDocument/didOpen` | Ingests document into memory and triggers analysis. |
| `textDocument/didChange` | Updates in-memory document state and re-analyzes. |
| `textDocument/didClose` | Clears document state and resets diagnostics. |

### Language Features
| Method | Description |
| :--- | :--- |
| `textDocument/publishDiagnostics` | Real-time syntax and type error notifications. |
| `textDocument/completion` | Context-aware autocompletion (keywords, stdlib, variables, functions). |
| `textDocument/hover` | Type information, function signatures, and docstrings. |
| `textDocument/definition` | Direct jump to symbol declaration (`SourceSpan`). |
| `textDocument/references` | Workspace-wide symbol usage locations. |
| `textDocument/documentSymbol` | Hierarchical outline view of functions, variables, and imports. |
| `workspace/symbol` | Workspace-wide fuzzy symbol query. |
| `textDocument/formatting` | Document reformatting using `Formatter::format_source`. |

---

## 3. Request & Response Examples

### Autocompletion Request (`textDocument/completion`)
```json
{
  "jsonrpc": "2.0",
  "id": 1,
  "method": "textDocument/completion",
  "params": {
    "textDocument": { "uri": "file:///project/src/main.nv" },
    "position": { "line": 5, "character": 4 }
  }
}
```

### Autocompletion Response
```json
{
  "jsonrpc": "2.0",
  "id": 1,
  "result": [
    {
      "label": "print",
      "kind": 3,
      "detail": "fn print(value)",
      "documentation": {
        "kind": "markdown",
        "value": "Prints value to standard output with newline"
      },
      "insertText": "print"
    }
  ]
}
```
