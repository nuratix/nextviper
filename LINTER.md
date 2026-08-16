# NextViper Static Linter

The NextViper static linter (`nextviper lint`) performs AST-level code quality inspections, flagging potential bugs, dead code paths, and performance antipatterns before runtime.

---

## 1. Diagnostic Checks & Warning Codes

### `NV3001` — Unused Variable
Triggered when a local variable is declared with `let` but is never referenced within its scope.
- **Remediation:** Remove the variable or prefix with `_` if intentionally unused.

```nextviper
// Warning: unused variable 'temp_result'
fn process_data():
    let temp_result = compute()
    return 100

// Clean: prefix with '_'
fn process_data():
    let _temp_result = compute()
    return 100
```

### `NV3002` — Unreachable Code
Triggered when statements occur in a block immediately following an unconditional `return`, `break`, or `continue`.
- **Remediation:** Remove the unreachable statements.

```nextviper
// Warning: unreachable code detected
fn calculate():
    return 42
    let x = 10 // Unreachable
```

### `NV3003` — Redundant Self-Comparison
Triggered when comparing an identifier directly to itself (e.g. `x == x` or `x != x`).
- **Remediation:** Remove redundant condition.

### `NV3004` — Redundant Constant Operation
Triggered on trivial operations that evaluate to their operand (e.g. `x + 0`, `x * 1`).
- **Remediation:** Simplify arithmetic expression.

### `NV3005` — Unused Import
Triggered when a module is imported but none of its symbols or functions are called.
- **Remediation:** Remove the unused `import` statement.

---

## 2. CLI Usage

```bash
# Lint entire project (src/ directory)
nextviper lint

# Lint specific files
nextviper lint src/server.nv src/models.nv
```
