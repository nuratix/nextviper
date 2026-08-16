# NextViper Official Code Formatter

NextViper includes an official, AST-driven source code formatter: `nextviper fmt`.

---

## 1. Principles

1. **Idempotence:** Formatting an already formatted file produces identical bytes (`fmt(fmt(src)) == fmt(src)`).
2. **AST & Comment Preservation:** Formats code structure while preserving all comments, docstrings, and string escape sequences.
3. **Deterministic Spacing:** 4-space indentation, consistent operator padding, standard function header layout, and blank line normalization.

---

## 2. Formatting Rules

- **Indentation:** 4 spaces per block level.
- **Operator Spacing:** Single space around binary operators (`+`, `-`, `*`, `/`, `==`, `!=`, `<`, `>`, `&&`, `||`, `|>`).
- **Function Declarations:** `fn name(param1: type, param2: type) -> return_type:`
- **Collections:** Space after comma in lists `[1, 2, 3]` and dictionaries `{"key": "value"}`.
- **Imports:** Grouped and aligned at the top of the file.

---

## 3. Usage & Modes

### Write in-place (default)
```bash
nextviper fmt
nextviper fmt src/main.nv
```

### Check mode (CI validation)
Validates formatting without writing changes. Returns non-zero exit code if unformatted files are found.
```bash
nextviper fmt --check
```

### Diff mode
Prints a unified diff showing exactly what changes would be made:
```bash
nextviper fmt --diff src/main.nv
```
