# NextViper Language Specification (v1.0.0 Apex)

**Status:** Official Stable Specification  
**Version:** 1.0.0  
**Target Execution Engines:** Tree-Walk Interpreter, Stack Bytecode Virtual Machine, Native LLVM/C Backend  

---

## 1. Syntax & Lexical Structure

### 1.1 Source Encoding & Identifiers
- Source files MUST be encoded in valid **UTF-8**.
- Keywords and identifiers are case-sensitive.
- Identifiers begin with an ASCII letter `[a-zA-Z]` or underscore `_`, followed by letters, digits `[0-9]`, or underscores.

### 1.2 Comments
- Single-line comments start with `//` and extend to the end of the line.
- Multi-line comments start with `/*` and end with `*/`. Multi-line comments can be arbitrarily nested.

### 1.3 Blocks and Delimiters
- NextViper uses Python/Nim-inspired colon `:` syntax followed by statements or blocks.
- Braces `{ ... }` are also accepted for C-family interoperability.
- Statement termination is newline or semicolon `;`.

---

## 2. Variables & Scoping

### 2.1 Variable Declaration
Variables are introduced with `let`:

```nextviper
let x = 10
let name: string = "NextViper"
let mut counter = 0
```

- Variable re-assignment modifies existing bindings in lexical scope.
- Lexical scoping: Inner blocks shadow outer bindings without destroying outer bindings.

---

## 3. Data Types & Type System

### 3.1 Primitives
- `int`: 64-bit signed two's complement integer (`int64_t`).
- `float`: 64-bit IEEE 754 floating-point (`double`).
- `bool`: Boolean `true` or `false`.
- `string`: Immutable UTF-8 byte sequence.
- `nil` / `null`: Unit null value.

### 3.2 Collections
- `list[T]`: Dynamically resizable array of values.
- `map[string, T]`: Associative hash table with string keys.

### 3.3 Scientific & AI Types
- `Tensor`: Multi-dimensional dense floating-point numerical tensor.
- `DataFrame`: Tabular structured series and columns.

### 3.4 Gradual Type Inference
Static type annotations are checked at compile/check time:
```nextviper
fn multiply(a: int, b: int) -> int:
    return a * b
```

---

## 4. Expressions & Operators

### 4.1 Operator Precedence Table (Highest to Lowest)

| Precedence | Operators | Description | Associativity |
| :--- | :--- | :--- | :--- |
| 1 (Highest) | `.` `()` `[]` | Member access, function call, index | Left |
| 2 | `!` `-` `~` | Logical NOT, Unary negation, Bitwise NOT | Right |
| 3 | `**` | Exponentiation | Right |
| 4 | `*` `/` `%` | Multiplication, Division, Modulo | Left |
| 5 | `+` `-` | Addition, Subtraction, String Concat | Left |
| 6 | `..` `..=` | Half-open and Inclusive Ranges | Left |
| 7 | `<` `<=` `>` `>=` | Relational Comparisons | Left |
| 8 | `==` `!=` | Equality and Inequality | Left |
| 9 | `&&` `and` | Logical AND | Left |
| 10 | `\|\|` `or` | Logical OR | Left |
| 11 | `\|>` | Pipeline Operator | Left |
| 12 (Lowest) | `=` `+=` `-=` `*=` `/=` | Assignment operators | Right |

---

## 5. Control Flow

### 5.1 Conditional Statements
```nextviper
if condition:
    // branch 1
elif other_condition:
    // branch 2
else:
    // fallback
```

### 5.2 Loop Statements

```nextviper
// Range loop
for i in 0..10:
    print(i)

// Collection iteration
for item in collection:
    print(item)

// While loop
while condition:
    if break_needed:
        break
    if skip_needed:
        continue
```

---

## 6. Functions, Lambdas & Closures

### 6.1 Function Declarations
```nextviper
fn add(a, b):
    return a + b

// Arrow expression syntax
fn double(x): x * 2

// Typed signature
fn divide(a: float, b: float) -> float:
    return a / b
```

### 6.2 First-Class Lambdas & Closures
```nextviper
let add_five = fn(x): x + 5
let values = [1, 2, 3].map(fn(x): x * 10)
```

---

## 7. Modules & Ecosystem

- Modules use explicit `export` and `import` semantics.
- Relative imports (`import "./utils.nv" as utils`) are canonicalized and path-isolated.
- Package manifests are stored in `nextviper.json`.
