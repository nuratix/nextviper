# NextViper Visual Studio Code Extension Guide

The official NextViper extension for Visual Studio Code is located in the [`editors/vscode/`](file:///root/nextviper/editors/vscode) directory.

---

## 1. Extension Capabilities

1. **Syntax Highlighting**: Comprehensive TextMate grammar matching keywords, functions, types, constants, strings, numbers, and operators.
2. **Real-Time Language Server (LSP)**:
   - Full support for `nextviper-lsp` binary.
   - Real-time compiler diagnostics with error squiggles.
   - Code completion for language keywords, built-ins, standard library modules (`std.io`, `std.fs`, `std.math`, etc.), and user symbols.
   - Hover cards showing function signatures and markdown docs.
   - Go to definition for functions and variables.
   - Find all references in workspace.
   - Document symbol outline.
3. **Format on Save**: Automatically runs `nextviper fmt` when saving `.nv` files.
4. **Commands**:
   - `NextViper: Restart Language Server`
   - `NextViper: Run Current File`
   - `NextViper: Check Project Syntax & Types`

---

## 2. Installation & Development

To load the extension in VS Code:

```bash
cd editors/vscode
npm install
npm run compile
```

Press `F5` in VS Code to launch the Extension Development Host window.
