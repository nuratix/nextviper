# NextViper Standard Library API Reference

Comprehensive specification and API reference for the NextViper 1.0 Standard Library.

---

## Table of Contents

1. [std.io](#1-stdio)
2. [std.fs](#2-stdfs)
3. [std.path](#3-stdpath)
4. [std.string](#4-stdstring)
5. [std.collections](#5-stdcollections)
6. [std.math](#6-stdmath)
7. [std.json](#7-stdjson)
8. [std.csv](#8-stdcsv)
9. [std.time](#9-stdtime)
10. [std.http](#10-stdhttp)
11. [std.process](#11-stdprocess)
12. [std.crypto](#12-stdcrypto)
13. [std.regex](#13-stdregex)
14. [std.random](#14-stdrandom)
15. [std.concurrency](#15-stdconcurrency)

---

## 1. std.io

Standard stream input and output primitives.

```nextviper
import std.io
```

### Functions

| Function | Signature | Description |
| :--- | :--- | :--- |
| `print` | `(...args) -> nil` | Prints values separated by spaces to stdout with a trailing newline. |
| `println` | `(...args) -> nil` | Alias for `print`. |
| `eprint` | `(...args) -> nil` | Prints values separated by spaces to stderr with a trailing newline. |
| `eprintln` | `(...args) -> nil` | Alias for `eprint`. |
| `write` | `(text: str) -> nil` | Writes raw string to stdout without a trailing newline. |
| `flush` | `() -> nil` | Flushes the stdout buffer. |
| `read_line` | `() -> str \| nil` | Reads one line from stdin. Returns `nil` on EOF. |
| `read_all` | `() -> str` | Reads entire input stream from stdin until EOF. |

---

## 2. std.fs

Safe cross-platform filesystem operations.

```nextviper
import std.fs
```

### Functions

| Function | Signature | Description |
| :--- | :--- | :--- |
| `read_text` / `read_file` | `(path: str) -> str` | Reads entire file contents as UTF-8 string. |
| `write_text` / `write_file`| `(path: str, content: str) -> bool` | Overwrites or creates file with given text content. |
| `append_text` / `append_file`| `(path: str, content: str) -> bool` | Appends text content to file. |
| `exists` | `(path: str) -> bool` | Returns `true` if path exists. |
| `is_file` | `(path: str) -> bool` | Returns `true` if path is a regular file. |
| `is_dir` | `(path: str) -> bool` | Returns `true` if path is a directory. |
| `list` / `read_dir` | `(path: str = ".") -> List<str>` | Returns sorted list of file and directory names. |
| `make_dir` / `mkdir` | `(path: str) -> bool` | Creates directory recursively. |
| `remove` / `delete_file` | `(path: str) -> bool` | Removes a single file. |
| `remove_dir` | `(path: str) -> int` | Removes directory and all children recursively. |
| `copy` | `(src: str, dst: str) -> bool` | Copies file to destination. |
| `move` / `rename` | `(src: str, dst: str) -> bool` | Moves or renames file to destination. |
| `size` | `(path: str) -> int` | Returns file size in bytes. |

---

## 3. std.path

Path manipulation, normalization, and inspection.

```nextviper
import std.path
```

### Functions & Properties

| Name | Type / Signature | Description |
| :--- | :--- | :--- |
| `join` | `(...parts: str) -> str` | Joins path components using the system separator. |
| `dirname` | `(path: str) -> str` | Returns the directory name of the path. |
| `basename` | `(path: str) -> str` | Returns the filename/basename component of path. |
| `extname` | `(path: str) -> str` | Returns the extension (including `.`, e.g. `.nv`). |
| `is_absolute` | `(path: str) -> bool` | Returns `true` if path is absolute. |
| `normalize` | `(path: str) -> str` | Canonicalizes `.` and `..` relative path segments. |
| `separator` | `str` | Platform path separator (`/` on Unix, `\\` on Windows). |

---

## 4. std.string

High-performance string routines and Unicode helpers.

```nextviper
import std.string
```

### Functions

| Function | Signature | Description |
| :--- | :--- | :--- |
| `split` | `(s: str, delim: str) -> List<str>` | Splits string by delimiter. |
| `join` | `(arr: List<str>, delim: str) -> str` | Joins list of strings by delimiter. |
| `trim` | `(s: str) -> str` | Strips leading and trailing whitespace. |
| `trim_start` | `(s: str) -> str` | Strips leading whitespace. |
| `trim_end` | `(s: str) -> str` | Strips trailing whitespace. |
| `to_upper` | `(s: str) -> str` | Converts string to uppercase. |
| `to_lower` | `(s: str) -> str` | Converts string to lowercase. |
| `starts_with`| `(s: str, prefix: str) -> bool` | Returns `true` if string starts with prefix. |
| `ends_with` | `(s: str, suffix: str) -> bool` | Returns `true` if string ends with suffix. |
| `contains` | `(s: str, substr: str) -> bool` | Returns `true` if string contains substring. |
| `index_of` | `(s: str, substr: str) -> int` | Returns 0-based index of substring or `-1`. |
| `replace` | `(s: str, old_s: str, new_s: str) -> str` | Replaces occurrences of `old_s` with `new_s`. |
| `len` | `(s: str) -> int` | Returns string character length. |

---

## 5. std.collections

Collection transformations, sorting, and data structure utilities.

```nextviper
import std.collections
```

### Functions

| Function | Signature | Description |
| :--- | :--- | :--- |
| `chunk` | `(arr: List<T>, size: int) -> List<List<T>>` | Splits list into chunks of given size. |
| `flatten` | `(arr: List<Any>) -> List<Any>` | Flattens 1 level of nested lists. |
| `unique` | `(arr: List<T>) -> List<T>` | Returns elements with duplicates removed. |
| `reverse` | `(arr: List<T>) -> List<T>` | Returns reversed copy of list. |
| `sort` | `(arr: List<T>) -> List<T>` | Returns numerically or alphabetically sorted copy. |
| `zip` | `(a: List<A>, b: List<B>) -> List<[A, B]>` | Zips two lists into pairs. |
| `merge` | `(a: Map<K, V>, b: Map<K, V>) -> Map<K, V>` | Merges two maps. |
| `keys` | `(m: Map<K, V>) -> List<str>` | Returns list of map keys. |
| `values` | `(m: Map<K, V>) -> List<V>` | Returns list of map values. |

---

## 6. std.math

Mathematical functions, trigonometry, and constants.

```nextviper
import std.math
```

### Functions & Constants

| Function | Signature | Description |
| :--- | :--- | :--- |
| `pi` | `float` | Archimedes constant (3.141592653589793...). |
| `e` | `float` | Euler constant (2.718281828459045...). |
| `sqrt` | `(x: num) -> float` | Square root. |
| `cbrt` | `(x: num) -> float` | Cube root. |
| `pow` | `(base: num, exp: num) -> float` | Power / exponentiation. |
| `abs` | `(x: num) -> num` | Absolute value. |
| `sin` / `cos` / `tan` | `(rad: num) -> float` | Trigonometric functions. |
| `asin` / `acos` / `atan` | `(x: num) -> float` | Inverse trigonometric functions. |
| `atan2` | `(y: num, x: num) -> float` | Arc tangent of two variables. |
| `floor` | `(x: num) -> int` | Floor rounding to integer. |
| `ceil` | `(x: num) -> int` | Ceiling rounding to integer. |
| `round` | `(x: num) -> int` | Nearest integer rounding. |
| `min` / `max` | `(a: num, b: num) -> num` | Minimum and maximum values. |
| `clamp` | `(v: num, lo: num, hi: num) -> float` | Clamps value between lower and upper bounds. |
| `deg2rad` / `rad2deg` | `(x: num) -> float` | Angle unit conversion. |

---

## 7. std.json

High-speed recursive JSON parser and serializer.

```nextviper
import std.json
```

### Functions

| Function | Signature | Description |
| :--- | :--- | :--- |
| `stringify` | `(value: Any, indent: int = 0) -> str` | Serializes NextViper value to JSON string. |
| `parse` | `(text: str) -> Any` | Parses JSON string into NextViper primitives, maps, and lists. |

---

## 8. std.csv

Tabular CSV file and data stream processing.

```nextviper
import std.csv
```

### Functions

| Function | Signature | Description |
| :--- | :--- | :--- |
| `parse` | `(text: str) -> List<List<str>>` | Parses CSV text into rows of columns. |
| `stringify` | `(rows: List<List<Any>>) -> str` | Serializes tabular rows into CSV string. |
| `read` | `(path: str) -> List<List<str>>` | Reads and parses CSV file. |

---

## 9. std.time

High-resolution clock, durations, and timestamps.

```nextviper
import std.time
```

### Functions

| Function | Signature | Description |
| :--- | :--- | :--- |
| `now` | `() -> float` | Current Unix timestamp in fractional seconds. |
| `now_ms` | `() -> int` | Current Unix timestamp in integer milliseconds. |
| `sleep` | `(ms: int) -> nil` | Sleeps the current thread for specified milliseconds. |
| `elapsed` | `(start_time: float) -> float` | Returns seconds elapsed since `start_time`. |
| `format` | `(time: float, fmt: str = "%Y-%m-%d %H:%M:%S") -> str` | Formats timestamp via `strftime`. |

---

## 10. std.http

Full HTTP client supporting GET, POST, PUT, DELETE, custom headers, and JSON responses.

```nextviper
import std.http
```

### Functions & Response Structure

| Function | Signature | Description |
| :--- | :--- | :--- |
| `get` | `(url: str, headers: Map = {}) -> Response` | Performs HTTP GET request. |
| `post` | `(url: str, body: Any = "", headers: Map = {}) -> Response` | Performs HTTP POST request. |
| `put` | `(url: str, body: Any = "", headers: Map = {}) -> Response` | Performs HTTP PUT request. |
| `delete` | `(url: str, headers: Map = {}) -> Response` | Performs HTTP DELETE request. |
| `request` | `(method: str, url: str, body: Any = "", headers: Map = {}) -> Response` | Performs generic HTTP request. |

#### `Response` Object Properties & Methods:
- `response.status`: `int` (e.g. `200`, `404`)
- `response.text` / `response.body`: `str`
- `response.headers`: `Map<str, str>`
- `response.ok`: `bool` (`true` if status in `[200, 299]`)
- `response.json()`: `() -> Any` (parses body text directly as JSON)

---

## 11. std.process

Process management, shell execution, and environment variables.

```nextviper
import std.process
```

### Functions

| Function | Signature | Description |
| :--- | :--- | :--- |
| `exec` | `(command: str) -> Map` | Runs shell command, returning `{"exit_code": int, "stdout": str, "stderr": str}`. |
| `exit` | `(code: int = 0) -> nil` | Terminates process with exit code. |
| `env` | `(name: str) -> str \| nil` | Reads environment variable. |
| `cwd` | `() -> str` | Returns current working directory. |
| `pid` | `() -> int` | Returns current process ID. |

---

## 12. std.crypto

Cryptographic hash algorithms, encodings, and secure random byte generation.

```nextviper
import std.crypto
```

### Functions

| Function | Signature | Description |
| :--- | :--- | :--- |
| `sha256` | `(text: str) -> str` | Computes 64-character SHA-256 hexadecimal digest. |
| `md5` | `(text: str) -> str` | Computes 32-character MD5 hexadecimal digest. |
| `base64_encode` | `(text: str) -> str` | Base64 encodes string. |
| `base64_decode` | `(encoded: str) -> str` | Base64 decodes string. |
| `random_bytes` | `(count: int) -> str` | Generates `count` bytes formatted as hex string. |

---

## 13. std.regex

Regular expression pattern matching and substitutions.

```nextviper
import std.regex
```

### Functions

| Function | Signature | Description |
| :--- | :--- | :--- |
| `test` | `(pattern: str, text: str) -> bool` | Checks if pattern matches text. |
| `match` | `(pattern: str, text: str) -> List<str> \| nil` | Returns captured match groups or `nil`. |
| `find_all` | `(pattern: str, text: str) -> List<str>` | Returns list of all substring matches. |
| `replace` | `(pattern: str, repl: str, text: str) -> str` | Replaces regex matches with replacement string. |

---

## 14. std.random

Pseudo-random numbers and collection sampling.

```nextviper
import std.random
```

### Functions

| Function | Signature | Description |
| :--- | :--- | :--- |
| `random` | `() -> float` | Returns random float in `[0.0, 1.0)`. |
| `randint` | `(min: int, max: int) -> int` | Returns random integer in `[min, max]`. |
| `uniform` | `(low: float, high: float) -> float` | Returns random float in `[low, high)`. |
| `choice` | `(arr: List<T>) -> T` | Selects a random element from non-empty list. |
| `shuffle` | `(arr: List<T>) -> List<T>` | Returns a shuffled copy of the list. |
| `seed` | `(val: int) -> nil` | Seeds the PRNG generator. |

---

## 15. std.concurrency

Message-passing channel communication.

```nextviper
import std.concurrency
```

### Functions & Channel Object

| Function | Signature | Description |
| :--- | :--- | :--- |
| `sleep` | `(ms: int) -> nil` | Sleeps current thread for `ms` milliseconds. |
| `channel` | `(capacity: int = 1024) -> Channel` | Creates a thread-safe message passing channel. |

#### `Channel` Object Methods:
- `ch.send(val: Any) -> bool`: Sends a value to the channel (blocks if full).
- `ch.recv() -> Any`: Receives value from channel (blocks until available).
- `ch.try_recv() -> Any | nil`: Non-blocking receive (returns `nil` if empty).
- `ch.len() -> int`: Returns current queue length.
- `ch.close() -> nil`: Closes the channel.
