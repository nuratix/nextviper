# NextViper Command-Line Interface (CLI) Manual

The `nextviper` CLI is the primary developer tool for building, running, formatting, testing, and managing NextViper packages.

---

## Command Reference

### `nextviper run <file.nv> [--watch]`
Executes a NextViper program file using the high-performance runtime.
- `--watch`, `-w`: Enters watch mode, continuously monitoring file modifications and automatically re-running.

```bash
nextviper run src/main.nv
nextviper run src/main.nv --watch
```

### `nextviper check [files...] [--format=json]`
Statically validates syntax, AST structure, imports, and type safety without generating an executable.
- If no files are specified, automatically inspects `src/main.nv` or all `.nv` source files in the project.
- Exits with code `0` on success, `1` on error.
- `--format=json`: Emits machine-readable diagnostic arrays for CI/CD integrations.

```bash
nextviper check
nextviper check --format=json
```

### `nextviper test [path]`
Discovers and executes all `.nv` test suites in `tests/` or the specified path, providing execution timings and pass/fail tallies.

```bash
nextviper test
nextviper test tests/test_demo.nv
```

### `nextviper fmt [options] <files...>`
Deterministically formats NextViper source code.
- `-w`, `--write`: Formats file in-place (default).
- `-c`, `--check`: Validates formatting without modifying files; exits non-zero if unformatted.
- `-d`, `--diff`: Emits unified diff of formatting adjustments.
- `--stdin`: Reads unformatted code from standard input and prints formatted code to stdout.

```bash
nextviper fmt src/main.nv
nextviper fmt --check src/
```

### `nextviper build <file.nv> [-o output] [--native|--bytecode] [--release]`
Compiles a NextViper program into an artifact:
- `--native`: Ahead-Of-Time native binary compilation.
- `--bytecode`: High-speed virtual machine chunk (`.nvc`).
- `--release`: Compiles with `-O3` optimizations.

```bash
nextviper build src/main.nv -o app --native
```

### `nextviper repl`
Launches the interactive NextViper evaluation shell.

```bash
nextviper repl
```

### `nextviper lsp`
Launches the NextViper Language Server Protocol daemon for editor integration.

```bash
nextviper lsp
```

### `nextviper info`
Displays current project manifest details (`nextviper.toml`), dependency tree status, lockfile status, standard library modules, and GPU compute availability.

```bash
nextviper info
```

### Package Management Commands
- `nextviper init [name]`: Initializes a new package with `nextviper.toml`.
- `nextviper add <pkg>`: Adds and resolves a dependency.
- `nextviper remove <pkg>`: Removes a declared dependency.
- `nextviper install`: Downloads and links all declared dependencies.
- `nextviper update`: Updates dependencies to matching SemVer releases.
- `nextviper list`: Displays resolved dependency tree.
- `nextviper publish`: Packages and publishes distribution archive.
