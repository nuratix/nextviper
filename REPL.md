# NextViper REPL (Read-Eval-Print Loop)

The NextViper REPL (`nextviper repl` or `nextviper` without arguments in an interactive TTY) provides an immediate, exploratory shell.

---

## 1. Features

- **Persistent Environment**: Variables, functions, and imported modules persist across commands within the active session.
- **Multi-Line Input**: Automatically detects open blocks (`:`, `{`, `(`, `[`) and provides continuation prompts (`... `).
- **Immediate Expression Printing**: Standalone expressions evaluate and print their result without requiring explicit `print()`.
- **ANSI Color Highlighting**: Error diagnostics are color-coded with line and column indicators.

---

## 2. Special REPL Commands

| Command | Description |
| :--- | :--- |
| `:help`, `:h` | Display REPL command summary |
| `:clear`, `:c` | Reset active evaluation environment |
| `:env` | Inspect currently bound global variables and functions |
| `:exit`, `:quit`, `:q` | Exit the REPL session |

---

## 3. Example Session

```
NextViper 1.0.0 (Apex) Interactive Shell
Type :help for commands or :exit to quit.

nv> let x = 42
42
nv> fn double(n):
...     return n * 2
nv> double(x)
84
nv> import std.math
nv> math.sqrt(64.0)
8.0
```
