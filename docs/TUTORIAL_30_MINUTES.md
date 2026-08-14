# Learn NextViper in 30 Minutes

Welcome to **NextViper 1.0** — a modern, fast, and ergonomic programming language designed for systems scripting, application logic, and high-performance data/AI engineering.

This guide will teach you everything you need to be productive in NextViper in just 30 minutes.

---

## 1. The Core Philosophy

NextViper combines the **expressiveness and simplicity of Python** with the **speed, predictability, and safety of compiled systems languages**.

- **Clean Syntax**: Intuitive colon-based blocks, clear expressions.
- **Gradual Typing**: Dynamic by default, optional static type annotations where performance and safety matter.
- **Native AI & Data**: Built-in high-performance tensors and tabular data pipelines.
- **Zero-Friction Tooling**: Formatter, compiler, type checker, REPL, and package manager built right into a single binary.

---

## 2. Hello, NextViper!

Let's start with the classic hello world:

```nextviper
// hello.nv
print("Hello, NextViper 1.0!")
```

Run it immediately from your terminal:

```bash
nextviper run hello.nv
```

---

## 3. Variables & Types

### 3.1 Variables & Mutability

Variables are declared with `let`. In NextViper, variables are mutable:

```nextviper
let name = "Junaid"
let mut age = 15      // 'mut' is optional documentation keyword for intent
age = age + 1

print("Name: " + name + ", Age: " + str(age))
```

### 3.2 Primitive Data Types

NextViper supports integers, floats, booleans, strings, and nil:

```nextviper
let count = 42                // int (64-bit integer)
let ratio = 3.14159           // float (64-bit IEEE 754)
let active = true             // bool
let greeting = "Welcome"      // string (UTF-8)
let empty = nil               // nil / null
```

### 3.3 Optional Static Type Annotations

You can optionally specify types for variables, function arguments, and return types. The type checker (`nextviper check`) verifies them at compile-time without slowing down runtime:

```nextviper
let total: int = 100
let rate: float = 0.05
let user_id: string = "NV-9901"
```

---

## 4. Collections: Lists & Maps

### 4.1 Lists (Arrays)

Lists are ordered, dynamic arrays:

```nextviper
let numbers = [1, 2, 3, 4, 5]

// Indexing (0-based)
print(numbers[0])   // 1

// Modifying and Appending
numbers.append(6)
numbers.push(7)

// Length
print(numbers.len()) // 7

// Slicing: [start..end]
let slice = numbers.slice(1, 4) // [2, 3, 4]

// Higher-order functional methods
let doubled = numbers.map(fn(x): x * 2)
let evens = numbers.filter(fn(x): x % 2 == 0)
let sum = numbers.reduce(0, fn(acc, x): acc + x)
```

### 4.2 Maps (Dictionaries / Objects)

Maps store key-value associations:

```nextviper
let user = {
    "name": "Junaid",
    "age": 15,
    "role": "Lead Architect"
}

// Accessing fields
print(user["name"])      // "Junaid"
print(user.name)         // Dot access: "Junaid"

// Updating fields
user["status"] = "Active"

// Checking keys and size
print(user.has("role"))  // true
print(user.keys())       // ["name", "age", "role", "status"]
```

---

## 5. Control Flow

### 5.1 If / Else Statements

Conditions do not require parentheses:

```nextviper
let score = 85

if score >= 90:
    print("Grade: A")
elif score >= 80:
    print("Grade: B")
else:
    print("Grade: C")
```

### 5.2 Modern Loop Syntax

NextViper features clean, modern loops:

#### Range Loops
```nextviper
// Half-open range (0 up to 5, excluding 5: 0, 1, 2, 3, 4)
for i in 0..5:
    print(i)

// Inclusive range (0 up to and including 5: 0, 1, 2, 3, 4, 5)
for i in 0..=5:
    print(i)
```

#### Iterating Over Collections
```nextviper
let fruits = ["Apple", "Banana", "Cherry"]

for fruit in fruits:
    print("Fruit: " + fruit)
```

#### While Loops with Break & Continue
```nextviper
let count = 0
while count < 10:
    count = count + 1
    if count == 3:
        continue
    if count == 8:
        break
    print(count)
```

---

## 6. Functions & Closures

### 6.1 Basic Functions

Functions are defined with `fn`:

```nextviper
fn add(a, b):
    return a + b

print(add(10, 20)) // 30
```

### 6.2 Typed Functions & Arrow Syntax

Functions can have static signatures and single-expression bodies:

```nextviper
// Single expression arrow body
fn square(x: int) -> int: x * x

// Typed function with block body
fn compute_tax(subtotal: float, rate: float) -> float:
    let tax = subtotal * rate
    return subtotal + tax

print(compute_tax(100.0, 0.08)) // 108.0
```

### 6.3 First-Class Lambdas & Closures

Functions can return functions and capture variables from enclosing scopes:

```nextviper
fn make_multiplier(factor):
    return fn(x): x * factor

let triple = make_multiplier(3)
print(triple(10)) // 30
```

---

## 7. Modules & Packages

NextViper provides a safe, modular import system.

### 7.1 Built-in Standard Library

```nextviper
import math
import data
import sys

let root = math.sqrt(64.0)
print("Square root: " + str(root))

// Selective import
from math import pi, sin
print("Sin of Pi/2: " + str(sin(pi / 2.0)))
```

### 7.2 Creating and Exporting Custom Modules

In `calculator.nv`:
```nextviper
export fn multiply(a, b):
    return a * b

export let VERSION = "1.0.0"
```

In `main.nv`:
```nextviper
import "./calculator.nv" as calc
from "./calculator.nv" import multiply

let result = calc.multiply(6, 7)
print("Result: " + str(result)) // 42
```

---

## 8. AI & Data Foundations

NextViper includes high-performance numerical tensors and data pipelines directly in the standard library.

### 8.1 Tabular Data Processing

```nextviper
import data

// Load tabular dataset
let df = data.load("dataset.csv")

// Clean and transform data
df.clean()
df.shuffle()

// View dataset stats
print("Rows: " + str(df.rows()) + ", Cols: " + str(df.cols()))
```

### 8.2 High-Performance Tensors & AI Models

```nextviper
import ai
import tensor

// Multi-dimensional tensor creation and operations
let a = tensor.tensor([[1.0, 2.0], [3.0, 4.0]])
let b = tensor.tensor([[5.0, 6.0], [7.0, 8.0]])
let c = tensor.matmul(a, b)

// AI Model Interface
let model = ai.load("classifier_model")
let predictions = model.predict(c)
print(predictions)
```

---

## 9. Developer Tooling (CLI)

NextViper includes world-class CLI developer tooling:

| Command | Action |
| :--- | :--- |
| `nextviper run main.nv` | Execute program via interpreter |
| `nextviper build main.nv -o prog` | Compile to standalone native binary |
| `nextviper check main.nv` | Statically validate syntax and types with rich diagnostics |
| `nextviper fmt main.nv` | Formats code deterministically |
| `nextviper repl` | Interactive REPL shell |
| `nextviper test` | Run test suites |
| `nextviper bench main.nv` | Multi-engine benchmark (Interpreter vs VM vs Native) |
| `nextviper package init my_app` | Initialize new package with `nextviper.json` manifest |

---

## 10. Summary & Next Steps

You now know the foundations of NextViper!

- Explore full language syntax: [Language Specification](file:///root/nextviper/docs/LANGUAGE_SPECIFICATION_1.0.md)
- Standard Library APIs: [Standard Library Reference](file:///root/nextviper/docs/STANDARD_LIBRARY_API.md)
- Examples: Check out the [`examples/`](file:///root/nextviper/examples) directory.
