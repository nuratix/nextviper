# NextViper VS Code Extension

Official Visual Studio Code extension providing first-class IDE support for the NextViper programming language.

## Features

- **Rich Syntax Highlighting**: Accurate TextMate grammar for language keywords, control flow, builtins, string escapes, number formats, and operators (`|>`, `=>`, `->`).
- **Language Server Protocol (LSP)**: Powered by the native C++ `nextviper-lsp` engine.
- **Real-Time Diagnostics**: Instant compiler syntax and static type diagnostics with precise line/column squiggles.
- **Context-Aware Autocompletion**: Keywords, variables, functions, standard library modules (`std.io`, `std.fs`, `std.math`), and packages.
- **Hover Documentation**: Signatures, parameters, return types, and markdown doc comments.
- **Go to Definition**: Jump directly to variable declarations and function declarations across files.
- **Find References**: Search identifier usage across the workspace.
- **Document Outline**: Structural symbol tree in the Outline view.
- **Automated Formatting**: Deterministic code formatting via `nextviper fmt`.

## Requirements

Ensure `nextviper` and `nextviper-lsp` are installed and available in your system PATH or compiled within your workspace `bin/` directory.

```bash
# Build from NextViper repository root
make -j$(nproc)
```
