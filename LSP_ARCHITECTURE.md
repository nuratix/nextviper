# NextViper Language Server Protocol (LSP) Architecture

NextViper provides an official implementation of the Microsoft Language Server Protocol (LSP 3.17) in `bin/nextviper-lsp` and `nextviper lsp`.

---

## 1. Capabilities Matrix

| LSP Feature | Method | Description |
|---|---|---|
| **Initialize & Handshake** | `initialize`, `initialized` | Capabilities negotiation |
| **Document Sync** | `textDocument/didOpen`, `textDocument/didChange` | Full & incremental document synchronization |
| **Real-time Diagnostics** | `textDocument/publishDiagnostics` | Live syntax, type, and lint error emission |
| **Autocompletion** | `textDocument/completion` | Context-aware keywords, variables, standard library symbols |
| **Hover Documentation** | `textDocument/hover` | Type signatures and doc comments on hover |
| **Go To Definition** | `textDocument/definition` | Navigates to variable, function, or type declarations |
| **Find References** | `textDocument/references` | Locates symbol usages across workspace |
| **Document Symbols** | `textDocument/documentSymbol` | Outline view of functions, variables, and modules |
| **Code Formatting** | `textDocument/formatting` | In-editor deterministic formatting via `nextviper fmt` |

---

## 2. Protocol Transport

The server communicates over standard input/output (`stdin`/`stdout`) using HTTP-style header framing:

```
Content-Length: 128\r\n
\r\n
{"jsonrpc":"2.0","id":1,"method":"initialize","params":{"capabilities":{}}}
```

---

## 3. Editor Integration (VS Code)

To use NextViper LSP in VS Code:
1. Ensure `nextviper` or `nextviper-lsp` is in your system `$PATH`.
2. Install the NextViper VS Code extension (`editors/vscode`).
3. Diagnostics, autocomplete, go-to-definition, and formatting activate automatically upon opening any `.nv` file.
