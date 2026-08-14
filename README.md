# NextViper Programming Language

<p align="center">
  <strong>A modern, easy-to-learn, fast general-purpose programming language.</strong><br>
  <em>Engineered for AI, high-performance data processing, automation, and systems development.</em>
</p>

---

## ⚡ Overview

**NextViper** is a next-generation programming language inspired by the readability and developer ergonomics of Python, but engineered from the ground up to **not** be a Python clone. It combines modern language design, explicit mutability, a native pipeline operator (`|>`), fast execution, and a compiler architecture designed for future LLVM native code generation and JIT optimization.

### Key Highlights
- **Clear & Modern Syntax**: Clean block scoping `{}` with optional semicolons, concise arrow functions (`=>`), and intuitive keywords.
- **Explicit Mutability**: Immutable by default with `let`, requiring `let mut` for mutable variables—preventing concurrency bugs and accidental state mutations.
- **AI & Data First-Class**: First-class pipeline operator (`|>`), vectorized array operations, concise lambdas, and numeric math built-ins.
- **Fast Execution & Native Future**: Core compiler and runtime implemented in high-performance **Modern C++20**, architected for native LLVM AOT and JIT compilation.
- **Aesthetic Diagnostics**: Rust-style compiler error messages with source line previews, visual underlines (`^^^^`), and contextual suggestions.
- **Zero-Dependency CLI Tooling**: Standalone `nextviper` CLI supporting file execution, syntax checking, AST dumping, token scanning, inline eval, and an interactive REPL.

---

## 🚀 Quick Start

### 1. Build NextViper

Requirements: A C++20 compatible compiler (e.g. `g++` 10+, `clang++` 12+) and GNU `make` or `cmake`.

```bash
# Clone and enter the repository
cd nextviper

# Build the nextviper CLI and test runner
make -j4
```

The compiled binary will be located at `bin/nextviper`.

---

### 2. Run Hello World

Execute the minimal Hello World program:

```bash
./bin/nextviper run examples/hello_world.nv
```

Output:
```
Hello, World! Welcome to NextViper.
```

---

### 3. Run the Test Suite

Execute the built-in test suite (unit tests and CLI integration tests):

```bash
make test
```

Output:
```
====================================================
  NextViper 0.1.0 Test Suite Runner
====================================================

[Lexer]
  ✓ PASS: ScanLiterals
  ✓ PASS: ScanKeywords
  ✓ PASS: ScanOperatorsAndPunctuation
  ✓ PASS: SkipComments
  ✓ PASS: LocationTracking
[Parser]
  ✓ PASS: ParseLetStatements
  ✓ PASS: ParseFunctionDeclarations
  ✓ PASS: ParseControlFlow
  ✓ PASS: ParsePipelineOperator
[Interpreter]
  ✓ PASS: ArithmeticAndPrecedence
  ✓ PASS: ImmutabilityEnforcement
  ✓ PASS: FunctionsAndRecursion
  ✓ PASS: ArrowFunctionsAndClosures
  ✓ PASS: LoopsAndBreakContinue
  ✓ PASS: PipelineOperator
  ✓ PASS: BuiltinFunctions
[Diagnostics]
  ✓ PASS: RenderErrorReport
  ✓ PASS: MultipleDiagnosticLevels

----------------------------------------------------
Tests Summary: 18 total | 18 passed | 0 failed
====================================================
```

---

## 📖 Language Syntax Tour

### Variables & Mutability
```nextviper
// Immutable by default
let language = "NextViper"
let version = "0.1.0"

// Explicitly mutable variable
let mut counter = 0
counter += 1
counter = counter * 10
print("Counter:", counter)
```

### Functions & Closures
```nextviper
// Standard function declaration
fn add(a: Int, b: Int) -> Int {
    return a + b
}

// Arrow function (concise expression body)
fn square(x) => x * x

// Closures & Higher-order functions
fn make_multiplier(factor) {
    fn mult(x) => x * factor
    return mult
}

let double = make_multiplier(2)
print("Double 21 is:", double(21)) // 42
```

### Data Pipeline Operator (`|>`)
```nextviper
fn double_val(x) => x * 2
fn add_bias(x) => x + 10

// Pipeline operator passes the left-hand result into the next function
let result = 15 |> double_val() |> add_bias()
print("Pipeline result:", result) // 40
```

### Control Flow
```nextviper
let score = 88

if score >= 90 {
    print("Grade: Excellent")
} else if score >= 75 {
    print("Grade: Good")
} else {
    print("Grade: Needs Improvement")
}

// Loops
for item in [10, 20, 30] {
    print("Item:", item)
}
```

---

## 🛠️ CLI Reference

```
Usage:
  nextviper [command] [options] [file.nv]

Commands:
  run <file.nv>       Execute a NextViper program file
  eval <code>, -e     Evaluate an inline NextViper code string
  check <file.nv>     Validate syntax and check for errors without running
  parse <file.nv>     Parse source file and display the Abstract Syntax Tree (AST)
  tokens <file.nv>    Scan source file and dump token stream
  repl                Start interactive REPL session
  version, -v         Display version information
  help, -h            Display help message
```

### Examples:
```bash
# Evaluate an inline script
./bin/nextviper -e 'let a = 10; let b = 32; print("Sum is:", a + b)'

# Check syntax without executing
./bin/nextviper check examples/data_pipeline.nv

# Inspect AST
./bin/nextviper parse examples/hello_world.nv

# Interactive REPL
./bin/nextviper repl
```

---

## 📂 Repository Structure

```
nextviper/
├── Makefile                      # Build system configuration
├── CMakeLists.txt                # CMake build configuration
├── README.md                     # Project overview and guide
├── SPECIFICATION.md              # Language specification document
├── ROADMAP.md                    # Multi-phase development roadmap
├── ARCHITECTURE.md               # Compiler architecture and design choices
├── LICENSE                       # MIT License
│
├── include/nextviper/            # Public & internal C++ header files
│   ├── common.hpp                # Source locations, spans, common types
│   ├── token.hpp                 # Token definitions and types
│   ├── lexer.hpp                 # Tokenizer & scanner
│   ├── ast.hpp                   # AST nodes and ASTPrinter visitor
│   ├── parser.hpp                # Recursive descent parser
│   ├── diagnostic.hpp            # Rust-style error reporter
│   ├── value.hpp                 # Dynamic runtime value system
│   ├── environment.hpp           # Lexical scope & mutability table
│   ├── interpreter.hpp           # Tree-walk execution engine & builtins
│   ├── repl.hpp                  # Interactive shell
│   └── version.hpp               # Version constants (v0.1.0)
│
├── src/                          # Implementation files
│   ├── main.cpp                  # CLI entrypoint
│   ├── token.cpp                 # Token string conversion
│   ├── diagnostic.cpp            # Diagnostic rendering and line excerpts
│   ├── lexer.cpp                 # Lexer scanner logic
│   ├── ast.cpp                   # AST printing logic
│   ├── parser.cpp                # Parser implementation
│   ├── value.cpp                 # Value operations and dynamic typing
│   ├── environment.cpp           # Scoping and mutability enforcement
│   ├── interpreter.cpp           # Runtime interpreter and standard library
│   └── repl.cpp                  # Interactive REPL implementation
│
├── tests/                        # Comprehensive test framework
│   ├── test_runner.hpp           # Assertion macros and test registry
│   ├── test_runner.cpp           # Test runner main
│   ├── test_lexer.cpp            # Lexer unit tests
│   ├── test_parser.cpp           # Parser unit tests
│   ├── test_interpreter.cpp      # Runtime execution tests
│   ├── test_diagnostics.cpp      # Error formatting tests
│   └── test_cli.sh               # CLI integration test suite
│
└── examples/                     # NextViper sample programs
    ├── hello_world.nv            # Minimal Hello World program
    ├── basics.nv                 # Variables, mutability, collections, control flow
    ├── functions.nv              # Functions, recursion, closures, arrow functions
    ├── math_ops.nv               # Numerical operations & math helpers
    ├── data_pipeline.nv          # Pipeline operator (|>) & data transformations
    └── fibonacci.nv              # Fibonacci sequence benchmark
```

---

## 📜 License

NextViper is released under the [MIT License](LICENSE).
