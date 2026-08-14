# NextViper Language Specification (v0.1.0)

**Language Name:** NextViper  
**Current Version:** 0.1.0 (`Vipera Genesis`)  
**File Extension:** `.nv`  
**MIME Type:** `text/x-nextviper`

---

## 1. Design Philosophy

NextViper is a modern, high-performance, general-purpose programming language engineered for developer ergonomics, clarity, speed, and seamless application across AI, numerical data processing, systems automation, and general software development.

### Guiding Principles
1. **Ergonomic Simplicity without Python Cloning**: Readability inspired by modern language developments (concise keywords, clean block structures, clear expression syntax) while avoiding Python's legacy pain points (no mandatory indentation, no GIL limitations, no ambiguous variable declarations).
2. **Explicit Mutability**: Variables are immutable by default (`let`), requiring explicit intent (`let mut`) to enable mutation, eliminating entire classes of concurrency and state bugs.
3. **Data & AI First-Class Primitives**: Native pipeline operator (`|>`), vectorized array operations, concise lambda expressions, and extensible tensor abstractions.
4. **Zero-Overhead Native Evolution**: An extensible AST and intermediate representation (IR) designed from day one to compile to high-speed bytecode VM instructions and direct LLVM native machine code.
5. **Aesthetic & Actionable Diagnostics**: Compiler errors provide visual source snippets, exact column underlines, and helpful suggestions.

---

## 2. Lexical Structure

### 2.1 Character Encoding & Identifiers
NextViper source files are encoded in UTF-8.
Identifiers begin with an ASCII letter (`a-z`, `A-Z`) or an underscore (`_`), followed by any combination of letters, digits, and underscores.

### 2.2 Keywords
The following identifiers are reserved keywords:
```nextviper
let      mut      fn       return   if       else
while    for      in       loop     break    continue
true     false    nil      match    struct   type
import   and      or       not
```

### 2.3 Comments
- **Single-line comments**: Begin with `//` and extend to the end of the line.
- **Multi-line comments**: Enclosed within `/*` and `*/` (nestable).

### 2.4 Literals
- **Integers**: Decimal (`42`, `1_000_000`), Hexadecimal (`0xFF`, `0x1A_2B`), Binary (`0b1010`, `0b1100_0011`).
- **Floats**: Standard decimal floats (`3.14`, `0.001`), scientific notation (`1e5`, `2.5e-3`).
- **Booleans**: `true` and `false`.
- **Nil**: `nil` represents the absence of a value.
- **Strings**: Double-quoted UTF-8 sequences (`"hello\nworld"`). Escape sequences supported: `\n`, `\t`, `\r`, `\"`, `\\`, `\0`.
- **Arrays**: Bracketed comma-separated elements (`[1, 2, 3, 4]`).
- **Objects**: Key-value pairs enclosed in braces (`{"host": "127.0.0.1", "port": 8080}`).

---

## 3. Variables and Scoping

### 3.1 Immutable Variable Declaration (`let`)
Variables declared with `let` are immutable and cannot be rebound or mutated:
```nextviper
let name = "NextViper"
let max_threads: Int = 16
// name = "Other"  <-- Compile/Runtime Error: cannot reassign to immutable variable
```

### 3.2 Mutable Variable Declaration (`let mut`)
Variables intended to change must explicitly specify `mut`:
```nextviper
let mut counter = 0
counter += 1
counter = counter * 2
```

### 3.3 Lexical Scoping
NextViper enforces block scoping delineated by curly braces `{ ... }`. Inner scopes can access and shadow variables from enclosing scopes.

---

## 4. Functions

### 4.1 Standard Block Functions
```nextviper
fn calculate_area(width: Float, height: Float) -> Float {
    return width * height
}
```

### 4.2 Arrow Expression Functions
For single-expression functions, the concise `=>` syntax avoids boilerplate:
```nextviper
fn square(x) => x * x
fn is_even(n) => n % 2 == 0
```

### 4.3 First-Class Functions & Closures
Functions can be assigned to variables, passed as arguments, and returned from other functions:
```nextviper
fn create_scaler(factor) {
    fn scale(x) => x * factor
    return scale
}

let scale_by_5 = create_scaler(5)
let result = scale_by_5(10) // 50
```

### 4.4 Anonymous Functions (Lambdas)
```nextviper
let double = fn(x) => x * 2
```

---

## 5. Control Flow

### 5.1 Conditional Statements (`if` / `else if` / `else`)
Parentheses around conditions are optional, while block braces `{}` are mandatory:
```nextviper
if score >= 90 {
    print("Grade: A")
} else if score >= 80 {
    print("Grade: B")
} else {
    print("Grade: C")
}
```

### 5.2 While Loops
```nextviper
let mut i = 0
while i < 5 {
    print("Iteration:", i)
    i += 1
}
```

### 5.3 For-In Loops
Iterates directly over collections (Arrays, Strings) or ranges:
```nextviper
for item in [10, 20, 30] {
    print(item)
}

for i in range(0, 10, 2) {
    print("Even index:", i)
}
```

### 5.4 Loop Controls
- `break`: Terminate the innermost loop immediately.
- `continue`: Skip the remainder of the current iteration.

---

## 6. Operators & Expressions

### 6.1 Pipeline Operator (`|>`)
The pipeline operator feeds the evaluation result of the left expression into the right function call as its primary argument:
```nextviper
let processed = raw_data
    |> clean_dataset()
    |> normalize_weights(scale: 1.5)
    |> compute_loss()
```

### 6.2 Operator Precedence Matrix

| Priority | Operators | Description |
|---|---|---|
| 1 (Highest) | `()`, `[]`, `.` | Function call, indexing, member access |
| 2 | `-`, `!`, `not` | Unary negation, logical NOT |
| 3 | `**`, `^` | Exponentiation / Power |
| 4 | `*`, `/`, `%` | Multiplication, Division, Modulo |
| 5 | `+`, `-` | Addition, Subtraction, Concatenation |
| 6 | `..`, `..=` | Range constructors |
| 7 | `<`, `<=`, `>`, `>=` | Relational comparisons |
| 8 | `==`, `!=` | Equality tests |
| 9 | `&&`, `and` | Logical AND |
| 10 | `\|\|`, `or` | Logical OR |
| 11 | `\|>` | Pipeline transformation |
| 12 (Lowest) | `=`, `+=`, `-=`, `*=`, `/=`, `%=` | Assignment |

---

## 7. Built-in Standard Functions

| Function | Signature | Description |
|---|---|---|
| `print(...)` | `print(...values: Any) -> Nil` | Prints values space-separated with a newline |
| `println(...)` | `println(...values: Any) -> Nil` | Alias for `print(...)` |
| `print_raw(...)` | `print_raw(...values: Any) -> Nil` | Prints values without a trailing newline |
| `len(col)` | `len(col: String \| Array \| Object) -> Int` | Returns number of elements |
| `typeof(val)` | `typeof(val: Any) -> String` | Returns dynamic type name |
| `range(...)` | `range(start: Int, end: Int, step?: Int) -> Array` | Generates a sequential integer array |
| `push(arr, v)`| `push(arr: Array, val: Any) -> Array` | Appends item to array |
| `pop(arr)` | `pop(arr: Array) -> Any` | Removes and returns last element |
| `clock()` | `clock() -> Float` | High-precision timestamp in seconds |
| `assert(c, m)`| `assert(cond: Bool, msg?: String) -> Bool` | Runtime assertion checker |
| `str(v)` | `str(val: Any) -> String` | Converts value to String |
| `int(v)` | `int(val: Any) -> Int` | Converts string/float/bool to Int |
| `float(v)` | `float(val: Any) -> Float` | Converts string/int/bool to Float |
| `abs(v)` | `abs(val: Number) -> Number` | Absolute value |
| `min(a, b)` | `min(a: Number, b: Number) -> Number` | Minimum of two numbers |
| `max(a, b)` | `max(a: Number, b: Number) -> Number` | Maximum of two numbers |
| `pow(a, b)` | `pow(base: Number, exp: Number) -> Number` | Computes $a^b$ |
| `sqrt(v)` | `sqrt(val: Number) -> Float` | Computes $\sqrt{v}$ |
| `input(p?)` | `input(prompt?: String) -> String` | Reads a line from stdin |

---

## 8. Error Reporting Specification

NextViper diagnostics adhere to visual readability standards:
```
error: cannot reassign to immutable variable 'data_matrix'
  --> models/neural_net.nv:42:5
   |
42 |     data_matrix = new_weights
   |     ^^^^^^^^^^^ use 'let mut data_matrix' to make it mutable
```
All errors must include:
- Severity level (`error`, `warning`, `note`, `help`)
- Target source file, line number, and column index
- Source code line excerpt with line numbering
- Visual caret/underline indicators precisely spanning the offending token
- Contextual suggestions or correction hints
