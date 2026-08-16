# NextViper CLI Reference Manual

The official command-line interface for NextViper (`nextviper`) provides end-to-end tooling for project initialization, static analysis, code formatting, linting, testing, building, benchmarking, and package management.

---

## 1. Global Usage

```bash
nextviper <COMMAND> [OPTIONS] [FILES...]
```

### Global Flags
- `--version`, `-v`: Print the NextViper toolchain version and release codename.
- `--help`, `-h`: Display the CLI help menu and command categories.

---

## 2. Core Development Commands

### `nextviper init [name]`
Initializes a new NextViper project directory with standard structure:
- `nextviper.toml` (project manifest)
- `src/main.nv` (application entrypoint)
- `tests/main_test.nv` (automated test suite)
- `README.md` (project documentation)
- `.gitignore` (ignore patterns)

```bash
nextviper init my_service
```

### `nextviper check [files...]`
Performs static lexing, parsing, and type checking without producing binaries or executing code. Returns exit code `0` on clean validation, `1` on error.
- `--format=json`: Output machine-readable JSON diagnostics.

```bash
nextviper check
nextviper check src/main.nv --format=json
```

### `nextviper fmt [options] [files...]`
Deterministically formats NextViper source code according to the official style guide.
- `-w, --write`: Write formatted changes in-place (default).
- `-c, --check`: Validate whether files are formatted without modifying them; returns non-zero exit code if unformatted.
- `-d, --diff`: Display unified diff of formatting modifications.

```bash
nextviper fmt
nextviper fmt --check
nextviper fmt --diff src/main.nv
```

### `nextviper lint [files...]`
Runs the static AST linter to detect:
- Unused local variables (`NV3001`)
- Unreachable statements following `return`/`break`/`continue` (`NV3002`)
- Redundant self-comparisons `x == x` (`NV3003`)
- Redundant arithmetic operations `x + 0`, `x * 1` (`NV3004`)
- Unused imports (`NV3005`)

```bash
nextviper lint
nextviper lint src/
```

### `nextviper test [path]`
Discovers and executes automated tests in `tests/` directory or a specific file. Returns exit code `0` if all tests pass, `1` if any test fails.

```bash
nextviper test
nextviper test tests/main_test.nv
```

### `nextviper build [file.nv] [options]`
Compiles a NextViper file or project into an executable artifact.
- `--native`: Compile directly to native machine code (ELF/Mach-O/PE).
- `--release`: Enable `-O3` compiler optimizations for maximum performance.
- `--debug`: Enable `-g -O0` debug symbols for GDB/LLDB debugging.
- `--bytecode`: Generate portable `.nvc` bytecode package.
- `-o <path>`: Specify custom output binary destination.

```bash
nextviper build --native --release
nextviper build src/main.nv -o dist/server --native
```

### `nextviper dev [file.nv]`
Launches development mode with automatic filesystem change detection. Watches source files and restarts the application instantly upon saving.

```bash
nextviper dev
nextviper dev src/server.nv
```

### `nextviper doctor`
Inspects the local system and verifies toolchain health:
- NextViper version & codename
- C++ backend compilers (`g++`, `clang++`)
- Vulkan GPU compute driver status
- OS & CPU architecture
- Package manager cache status & registry connectivity

```bash
nextviper doctor
```

### `nextviper doc [path]`
Generates Markdown API reference documentation from module declarations, function signatures, and doc comments.

```bash
nextviper doc
nextviper doc src/
```

### `nextviper clean`
Safely removes build artifacts (`build/`, `dist/`, `.nvc` bytecode files) and temporary caches without deleting source files.

```bash
nextviper clean
```

### `nextviper bench <file.nv>`
Runs repeatable execution benchmarks comparing the Tree-Walk Interpreter, Bytecode VM, and Native AOT Compiler with average latency and speedup metrics.

```bash
nextviper bench benchmarks/matrix_bench.nv
```

---

## 3. Package Management Commands

- `nextviper add <pkg>`: Add a dependency to `nextviper.toml` and download it.
- `nextviper remove <pkg>`: Remove a dependency from manifest and lockfile.
- `nextviper install`: Install dependencies declared in `nextviper.toml` and verify checksums.
- `nextviper update [pkg]`: Update dependencies to latest compatible SemVer releases.
- `nextviper list`: View dependency tree and package integrity hashes.
- `nextviper publish [--dry-run]`: Package and publish distribution bundle to registry.
- `nextviper search <query>`: Search the package registry for available modules.
