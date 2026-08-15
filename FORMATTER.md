# NextViper Code Formatter Specification

The NextViper Formatter (`nextviper fmt`) ensures consistent, idiomatic, and deterministic source code style across the entire ecosystem.

---

## 1. Core Guarantees

1. **Idempotency**: Running `nextviper fmt` repeatedly produces identical results:
   $$\text{fmt}(\text{fmt}(S)) \equiv \text{fmt}(S)$$
2. **AST & Token Safety**: Formatting never alters the semantic execution or AST structure of code.
3. **Comment Preservation**: Line comments (`//`) and docstrings are strictly preserved in their relative position.
4. **CI Compatibility**: `nextviper fmt --check` returns non-zero exit codes when files deviate from formatted style.

---

## 2. Formatting Rules

- **Indentation**: 4 spaces per nesting level. Tabs are automatically converted to spaces.
- **Binary Operators**: Single spaces around binary operators (`+`, `-`, `*`, `/`, `==`, `!=`, `<`, `<=`, `>`, `>=`, `&&`, `||`, `|>`, `->`, `=>`, `=`).
- **Ranges**: Compact formatting without spaces around `..` and `..=`.
- **Delimiter Spacing**: Comma followed by a single space (`, `). Colons on type annotations formatted as `: `.
- **Trailing Whitespace**: Automatically stripped on all lines.
- **File End**: Files always terminate with a single newline (`\n`).

---

## 3. Usage Examples

```bash
# In-place formatting of all files in project
nextviper fmt src/ tests/

# Check formatting in CI pipeline
nextviper fmt --check

# Format code via stdin
cat unformatted.nv | nextviper fmt --stdin
```
