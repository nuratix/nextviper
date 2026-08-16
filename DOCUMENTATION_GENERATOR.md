# NextViper Documentation Generator

NextViper includes a built-in documentation generator (`nextviper doc`) that parses source files and extracts API documentation directly from source code and doc comments.

---

## 1. Syntax & Doc Comments

Functions and modules documented with `//` comments preceding their declarations are extracted automatically:

```nextviper
// Calculates the total sum of two integers.
// Parameters:
//   a - First operand
//   b - Second operand
export fn add(a: int, b: int) -> int:
    return a + b
```

---

## 2. Usage

```bash
# Generate documentation for all files in src/
nextviper doc

# Generate documentation for a specific directory or file
nextviper doc src/controllers/

# Pipe documentation to markdown file
nextviper doc > API.md
```
