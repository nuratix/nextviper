---
name: Compiler or Runtime Diagnostic Bug
about: Report a crash, incorrect codegen, or type checker issue in NextViper
title: '[COMPILER]: '
labels: compiler, bug
assignees: ''
---

**Compiler Subsystem**
- [ ] Lexer / Tokenizer
- [ ] Parser / AST
- [ ] Type Checker / Inferrer
- [ ] Interpreter / VM
- [ ] Native Code Generator (`nextviper build`)
- [ ] Vulkan GPU Backend
- [ ] LSP Daemon (`nextviper-lsp`)

**NextViper Version**
```text
nextviper --version
```

**Minimal Reproducible Example**
```nv
# Minimal .nv snippet causing the bug
```

**Command Executed**
```bash
nextviper run reproduction.nv
# or nextviper build reproduction.nv
```

**Observed Diagnostics / Crash Trace**
```text
Paste compiler output or stack trace
```

**Expected Result**
What the compiler should have reported or generated.
