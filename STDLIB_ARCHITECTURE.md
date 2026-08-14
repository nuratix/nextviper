# NextViper Standard Library Architecture Specification

---

## 1. Architectural Philosophy

NextViper’s standard library is designed as a **stable, modular, and high-performance API layer** that sits directly on top of the NextViper runtime. It adheres strictly to the following foundational principles:

1. **Separation of Concerns**: Core language grammar remains pure, minimal, and orthogonal. High-level capabilities (filesystem, network, JSON serialization, process control, cryptography) are provided as standard modules rather than built-in keywords.
2. **Dual-Engine Consistency**: Every standard library function behaves identically whether executed via the tree-walk/VM interpreter or compiled through the AOT native backend.
3. **Zero-Dependency Portability**: Built using Modern C++20 standard capabilities with self-contained cryptographic algorithms (FIPS 180-2 SHA-256, RFC 1321 MD5), recursive JSON engines, and standardized POSIX/Windows filesystem integrations.
4. **Resilient Error Handling**: Failures (e.g. non-existent files, malformed JSON, network errors) trigger structured runtime diagnostics with exact source spans rather than unhandled native crashes.

---

## 2. Layered Architecture

```
+-------------------------------------------------------------------------+
|                  User NextViper Source Code (*.nv)                      |
|       import std.fs    import std.json    import std.http    ...        |
+------------------------------------+------------------------------------+
                                     |
                                     v
+-------------------------------------------------------------------------+
|                Module Manager & Name Resolution Layer                   |
|  - Canonical Search Path Resolution ("./std", "./modules", "./packages")|
|  - Circular Dependency Detection & Caching                              |
|  - Namespace Isolation & Export Packaging                               |
+------------------------------------+------------------------------------+
                                     |
               +---------------------+---------------------+
               |                                           |
               v                                           v
+-----------------------------+             +-----------------------------+
|    Interpreter Runtime      |             |     Native AOT Compiler     |
| - Value Tagged Union        |             | - 3AC Typed IR Lowering     |
| - Native C++ Function Thunks|             | - C Runtime Linkage Library |
| - First-Class Closures      |             | - Machine Binary Generation |
+-----------------------------+             +-----------------------------+
               |                                           |
               +---------------------+---------------------+
                                     |
                                     v
+-------------------------------------------------------------------------+
|             Standard Platform & OS Abstraction Layer                    |
|    POSIX / Win32 / Libc / Sockets / Filesystem / Microsecond Clocks     |
+-------------------------------------------------------------------------+
```

---

## 3. Standard Library Modules

The standard library encompasses 15 core domains organized under `std/`:

| Module | Canonical Import | Description |
| :--- | :--- | :--- |
| **`std.io`** | `import std.io` | Standard stream I/O (`print`, `println`, `eprint`, `eprintln`, `read_line`, `read_all`, `flush`). |
| **`std.fs`** | `import std.fs` | Safe filesystem operations (`read_text`, `write_text`, `append_text`, `exists`, `is_file`, `is_dir`, `list`, `make_dir`, `remove`, `copy`, `move`, `size`). |
| **`std.path`** | `import std.path` | Path normalization and manipulation (`join`, `dirname`, `basename`, `extname`, `is_absolute`, `normalize`). |
| **`std.string`**| `import std.string` | High-speed string routines (`split`, `join`, `trim`, `to_upper`, `to_lower`, `starts_with`, `ends_with`, `contains`, `replace`, `len`). |
| **`std.collections`**| `import std.collections` | Data structure utilities (`chunk`, `flatten`, `unique`, `reverse`, `sort`, `zip`, `merge`, `keys`, `values`). |
| **`std.math`** | `import std.math` | Mathematical functions & constants (`sqrt`, `cbrt`, `pow`, `sin`, `cos`, `tan`, `log`, `floor`, `ceil`, `round`, `min`, `max`, `clamp`, `pi`, `e`). |
| **`std.json`** | `import std.json` | High-speed JSON serialization and recursive descent parsing (`stringify`, `parse`). |
| **`std.csv`** | `import std.csv` | Tabular CSV serialization and parsing (`parse`, `stringify`, `read`). |
| **`std.time`** | `import std.time` | Timestamps, durations, formatting, and sleeping (`now`, `now_ms`, `sleep`, `elapsed`, `format`). |
| **`std.http`** | `import std.http` | HTTP client with status codes, headers, response text, and native `.json()` body parser (`get`, `post`, `put`, `delete`, `request`). |
| **`std.process`**| `import std.process`| Process spawning, environment variables, working directory, and PID (`exec`, `exit`, `env`, `cwd`, `pid`). |
| **`std.crypto`** | `import std.crypto` | Cryptographic digests and encodings (`sha256`, `md5`, `base64_encode`, `base64_decode`, `random_bytes`). |
| **`std.regex`** | `import std.regex` | Regular expressions pattern matching and replacement (`test`, `match`, `find_all`, `replace`). |
| **`std.random`**| `import std.random` | High-quality pseudo-random number distributions (`random`, `randint`, `uniform`, `choice`, `shuffle`, `seed`). |
| **`std.concurrency`**| `import std.concurrency` | Message passing channels and task synchronization (`channel`, `sleep`). |

---

## 4. Module Resolution & Security Model

The [`ModuleManager`](file:///root/nextviper/src/module.cpp) enforces security boundaries during import resolution:

1. **Path Canonicalization**: Resolves paths to absolute canonical forms preventing directory traversal attacks (`../` escaping project roots).
2. **Cycle Prevention**: Maintains an active loading set `loading_modules_` detecting circular import dependencies at compile/load time.
3. **Search Path Precedence**:
   - Built-in runtime modules (`std.*`)
   - Current file directory
   - Standard library root (`./std`)
   - Configured search paths (`./modules`, `./nextviper_modules`, `./packages`)
4. **Namespace Encapsulation**: Module execution occurs in an isolated lexical environment frame; exported symbols are packaged into an immutable `Value::OBJECT` preventing namespace pollution.

---

## 5. Dual-Engine Equivalence & Integration

Both execution backends provide identical access to standard library functions:

- **Interpreter Mode**: Lowered to `NativeFunction` objects in `Value` with typed argument validation.
- **Native Compiled Mode**: Lowered in Three-Address-Code (3AC) Intermediate Representation to C-runtime ABI function declarations (`nv_fn_math_*`, `nv_fn_string_*`, `nv_fn_time_*`).
- **Equivalence Verification**: Automated test suites in [`tests/test_stdlib.cpp`](file:///root/nextviper/tests/test_stdlib.cpp) assert exact stdout and return value parity across both engines.
