# NextViper Programming Language (v1.0.0 Apex)

<p align="center">
  <strong>A modern, easy-to-learn, lightning-fast general-purpose programming language.</strong><br>
  <em>Engineered for AI, high-performance data processing, automation, and systems development.</em>
</p>

<p align="center">
  <a href="docs/TUTORIAL_30_MINUTES.md"><b>Learn in 30 Minutes</b></a> •
  <a href="docs/LANGUAGE_SPECIFICATION_1.0.md"><b>Language Spec</b></a> •
  <a href="docs/STANDARD_LIBRARY_API.md"><b>Standard Library API</b></a> •
  <a href="docs/CROSS_PLATFORM_BUILD.md"><b>Build Guide</b></a> •
  <a href="PERFORMANCE.md"><b>Benchmarks</b></a> •
  <a href="docs/SECURITY.md"><b>Security Audit</b></a>
</p>

---

## ⚡ Overview

**NextViper** is a production-grade 1.0 programming language engineered for developer ergonomics, absolute reliability, and raw performance. Inspired by the clean readability of modern languages without being a clone, NextViper offers:

- **Clean, Expressive Syntax**: Python-like clean block scoping `:` and `{}` blocks, arrow expressions (`=>`), explicit mutability (`let mut`), and native pipeline chaining (`|>`).
- **High-Performance Architecture**: Dual-engine runtime featuring a high-speed tree-walk interpreter, register-based VM, and a native AOT compiler with constant folding and dead-code elimination.
- **First-Class AI & Data Science Standard Library**: Native high-performance N-dimensional tensors (`tensor.tensor`, `tensor.matmul`), tabular DataFrames (`data.from_csv`, `data.DataFrame`), and neural building blocks (`ai.Linear`, `ai.SGD`, `ai.Adam`).
- **Professional Tooling Built-in**: Zero-dependency CLI with deterministic code formatting (`fmt`), package management (`package`), test runner (`test`), static type checking (`check`), and an interactive REPL (`repl`).
- **Rock-Solid Security**: Memory-safe execution, recursion bounds, CPU timeout protections, isolated file paths, and zero arbitrary code execution during package resolution.

---

## 🚀 Quick Start

### 1. Build NextViper

NextViper builds out of the box with any C++20 compiler (`g++` 10+, `clang++` 12+, Apple Clang 13+, or MSVC 2019+):

```bash
# Build the NextViper toolchain and test suite
make -j4

# Or using CMake
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j4
```

The unified CLI binary is generated at `bin/nextviper`.

---

### 2. Run Hello World & Tutorials

```bash
# Run Hello World
./bin/nextviper run examples/hello_world.nv

# Run the 30-minute tutorial suite
./bin/nextviper run examples/tutorials/01_basics.nv
./bin/nextviper run examples/tutorials/06_ai_and_data.nv
```

---

### 3. Run Test Suite

```bash
make test
```

Runs 75+ unit tests, fuzz testing protection checks, and CLI integration tests with 100% pass rate.

---

## 📚 Documentation Index

| Guide | Description |
| :--- | :--- |
| [**Learn NextViper in 30 Minutes**](docs/TUTORIAL_30_MINUTES.md) | Step-by-step hands-on tutorial covering all language features. |
| [**Language Specification 1.0**](docs/LANGUAGE_SPECIFICATION_1.0.md) | Formal grammar, lexical rules, types, semantics, and execution model. |
| [**Standard Library API**](docs/STANDARD_LIBRARY_API.md) | Full API reference for `math`, `data`, `tensor`, `ai`, `sys`, and collections. |
| [**Performance & Benchmarking**](PERFORMANCE.md) | Detailed benchmark methodology, hardware specs, and optimization analysis. |
| [**Security & Reliability Audit**](docs/SECURITY.md) | Memory safety review, DoS protection, recursion bounds, and fuzz testing report. |
| [**Cross-Platform Build Guide**](docs/CROSS_PLATFORM_BUILD.md) | Build instructions for Linux, macOS, Windows (MSVC & MinGW), and Docker. |

---

## 📖 Language Syntax Tour

### 1. Variables & Explicit Mutability
```nextviper
// Immutable by default
let language = "NextViper"
let version = "1.0.0"

// Explicitly mutable with let mut
let mut total = 100
total += 25
print("Total:", total)
```

### 2. Modern Loops & Ranges
```nextviper
// Half-open range (0..5 -> 0, 1, 2, 3, 4)
for i in 0..5:
    print(i)

// Inclusive range (0..=5 -> 0, 1, 2, 3, 4, 5)
for i in 0..=5:
    print(i)

// Collection iteration
let items = ["apple", "banana", "cherry"]
for item in items:
    print("Fruit:", item)

// While loop with break/continue
let mut n = 0
while n < 10:
    n += 1
    if n == 5:
        continue
    print(n)
```

### 3. Functions, Arrow Syntax & Closures
```nextviper
// Concise arrow function
fn square(x) => x * x

// Typed function
fn add(a: int, b: int) -> int:
    return a + b

// First-class closures
fn make_adder(bias):
    return fn(x) => x + bias

let add_10 = make_adder(10)
print(add_10(5)) // 15
```

### 4. Data Pipeline Operator (`|>`)
```nextviper
fn clean(s) => s.trim()
fn shout(s) => s.to_upper() + "!"

let result = "  hello world  " |> clean() |> shout()
print(result) // "HELLO WORLD!"
```

### 5. Tabular Data & AI Tensors
```nextviper
import data
import tensor
import ai

// Load and manipulate dataset
let df = data.from_csv("dataset.csv")
print("Rows:", df.rows(), "Cols:", df.cols())

// N-Dimensional Tensors & Matrix Multiplication
let A = tensor.tensor([[1.0, 2.0], [3.0, 4.0]])
let B = tensor.tensor([[5.0, 6.0], [7.0, 8.0]])
let C = tensor.matmul(A, B)

// Neural Network Linear Layer
let model = ai.create_linear(4, 2)
let output = model.forward(A)
```

---

## 🛠️ CLI Reference

NextViper comes equipped with professional developer tooling:

```
Usage: nextviper <command> [arguments...] [options]

Commands:
  run <file.nv>            Execute a NextViper source file
  build <file.nv>          Compile to native binary executable
  check <file.nv>          Validate syntax and types (--format=text|json)
  fmt <files...>           Deterministic source code formatter (--write, --check)
  test [directory]         Run automated test files (*_test.nv, test_*.nv)
  package <new|build>      Manage NextViper packages and dependencies
  repl                     Launch interactive REPL session
  eval <code>, -e          Evaluate inline code snippet
  parse <file.nv>          Output Abstract Syntax Tree (AST)
  tokens <file.nv>         Output lexical token stream
  version, -v              Print version information
  help, -h                 Display command help
```

### Aesthetic Diagnostics
NextViper features Rust-style error reports with colored underlines and actionable hints:

```
error[NV102]:
    unknown variable `total`

    --> main.nv:12:5

    12 | print(total)
       |       ^^^^^

    help: define `total` before using it
```

---

## 📂 Repository Structure

```
nextviper/
├── bin/                          # Compiled binaries (nextviper, test_runner)
├── docs/                         # Official 1.0 Documentation Suite
│   ├── TUTORIAL_30_MINUTES.md    # 30-Minute Beginner-to-Advanced Tutorial
│   ├── LANGUAGE_SPECIFICATION_1.0.md # Formal Language Specification
│   ├── STANDARD_LIBRARY_API.md   # Standard Library Reference
│   ├── CROSS_PLATFORM_BUILD.md   # Multi-Platform Build Instructions
│   └── SECURITY.md               # Security & Reliability Audit
├── examples/                     # Example programs
│   ├── hello_world.nv            # Minimal example
│   ├── data_pipeline.nv          # Pipeline operator showcase
│   └── tutorials/                # Structured tutorial scripts
│       ├── 01_basics.nv
│       ├── 02_collections.nv
│       ├── 03_loops_and_control.nv
│       ├── 04_functions_and_closures.nv
│       ├── 05_modules_and_imports.nv
│       └── 06_ai_and_data.nv
├── benchmarks/                   # Performance benchmarking suite
│   └── benchmark_suite.nv        # Comprehensive runtime benchmarks
├── include/nextviper/            # C++ Header declarations
├── src/                          # C++ Implementation source files
├── tests/                        # Unit, integration, and fuzz test suites
├── PERFORMANCE.md                # Benchmark analysis & methodology
├── Makefile                      # Standard build system
├── CMakeLists.txt                # CMake build configuration
└── LICENSE                       # MIT License
```

---

## 📜 License

NextViper is open-source software released under the [MIT License](LICENSE).
