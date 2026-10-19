# NextViper

NextViper is an **experimental programming language** with Python-like syntax, an interpreter, an in-memory bytecode VM, and a native compiler. It is developed by Nuratix LLC and licensed under Apache-2.0.

The version string is currently 1.0.0 (Apex). That version number is not a production-readiness guarantee. The project is suitable for learning and evaluation; production reliability, security, portability, and competitive performance have not been established.

## Build on Linux

A C++20 compiler, Make or CMake, Vulkan development libraries, PostgreSQL development libraries, Python 3, and curl are required. These are dependencies even when a program does not use GPU or database APIs.

```sh
sudo apt-get update
sudo apt-get install -y build-essential cmake libvulkan-dev libpq-dev python3 curl
make -j2
make test
```

Or use CMake:

```sh
cmake -S . -B build-cmake -DCMAKE_BUILD_TYPE=Release
cmake --build build-cmake -j2
ctest --test-dir build-cmake --output-on-failure
```

The Make binaries are in `bin/`; CMake binaries are in the build directory's `bin/` subdirectory. Run tests in your own environment. Test results cover the exercised cases, not all possible programs.

## Try the language

```nextviper
fn make_adder(bias):
    return fn(x) => x + bias

let add_ten = make_adder(10)
let mut total = 0
for i in 0..5:
    total += add_ten(i)
print(total)
```

```sh
./bin/nextviper run examples/hello_world.nv
./bin/nextviper check examples/hello_world.nv
./bin/nextviper build examples/hello_world.nv -o /tmp/nextviper-hello
/tmp/nextviper-hello
```

## Execution and tools

- `run file.nv` uses the tree-walk interpreter. `run file.nv --vm` selects the experimental VM.
- `build file.nv -o program` generates C++20 using tagged runtime values and invokes a host compiler. Native compilation supports a subset of the interpreter's libraries and syntax. Unsupported operations must report errors.
- `build --bytecode` is unavailable: persistent bytecode serialization/loading has not been implemented. The VM executes compiled bytecode in memory.
- `check` provides partial static checking. It does not prove program correctness or check every dynamic library call.
- `fmt` conservatively adjusts whitespace while preserving the token stream and original indentation. It is a line-based formatter, not an AST formatter.
- `repl`, `lint`, `test`, and `nextviper-lsp` provide experimental development tools.
- Tensor, data, neural-network, Vulkan, HTTP, and PostgreSQL implementations exist. Their presence does not establish production readiness. HTTP client calls require the external curl executable.

## Status and evidence

[Current status and known limitations](CURRENT_STATUS.md) is the maintained readiness reference. [Hardening changes](HARDENING.md) explains the reproduced issues and their regression tests. [Security](SECURITY.md) describes the trust boundary and remaining risks. [Testing](TESTING.md) explains how to write tests that actually fail.

Historical audit, release-readiness, and benchmark claims have been withdrawn where they were not supported. No general speed advantage over Python, Go, Rust, or C++ is claimed. See [performance methodology](PERFORMANCE.md).

Linux is the primary development target. Windows and macOS support must be validated separately; some runtime paths are POSIX-only. Published packages, installers, registries, and hosted services have not been validated by this source review.

## GitHub language recognition

NextViper is not yet included in GitHub Linguist. The repository includes a TextMate grammar, but GitHub recognition requires an upstream contribution and sufficient independent adoption. See [GitHub language support](docs/GITHUB_LANGUAGE_SUPPORT.md) for the requirements and submission checklist.
