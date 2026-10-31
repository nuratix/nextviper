# NextViper

NextViper is an experimental programming language with Python-like syntax,
lexical closures, collections, an interpreter, and development tools. It is
built in C++20 and licensed under Apache-2.0.

The first release target is **small Linux scripts, command-line tools, and HTTP
backends with PostgreSQL**. The repository includes a complete task API with
persistent storage, validation, and token authentication. Start with the default
interpreter. The VM, native compiler, AI, and GPU features have separate limits.

The version string remains `1.0.0 (Apex)` for compatibility. It does not certify
production reliability, API stability, or suitability for every application.
Read [current support and limitations](CURRENT_STATUS.md) before adopting it.

## Build and install on Linux

A C++20 GCC/Clang compiler, Make or CMake, Vulkan and PostgreSQL development
libraries, libcurl 7.85+, Python 3, curl, and OpenSSL are required. On Ubuntu 24.04:

```sh
sudo apt-get update
sudo apt-get install -y build-essential cmake libvulkan-dev libpq-dev libcurl4-openssl-dev python3 curl openssl
git clone https://github.com/nuratix/nextviper.git
cd nextviper
make -j2 test
bash install.sh
```

The installer installs both `nextviper` and `nextviper-lsp` into `~/.local/bin`.
Add that directory to your shell's PATH. Set `NEXTVIPER_INSTALL_DIR` to choose
another binary destination. Installation works when invoked outside the checkout.
To remove those two binaries, run `bash install.sh --uninstall` with the same
destination setting.

Alternatively, build with CMake:

```sh
cmake -S . -B build-cmake -DCMAKE_BUILD_TYPE=Release
cmake --build build-cmake -j2
ctest --test-dir build-cmake --output-on-failure
cmake --install build-cmake --prefix "$HOME/.local"
```

Build dependencies currently include GPU and database libraries even for scripts
that do not use them. Linux ARM64 and x86_64 have CI jobs; inspect their latest
results for your commit. Windows and macOS are not supported release targets yet.

## Your first program

Save this as `hello.nv`:

```nextviper
print("Hello, world!")
```

```sh
nextviper run hello.nv
# Output: Hello, world!
nextviper check hello.nv
nextviper repl
```

Try the [three tested practical applications](examples/practical) or the
[beginner tutorials](examples/tutorials) next. Integer arithmetic uses signed
64-bit values and reports overflow instead of wrapping. Collection iteration
visits a snapshot. Strings and string slices operate on bytes.

The [engineering audit](docs/audits/nextviper-engineering-audit.md) and
[validation record](docs/audits/nextviper-validation.md) distinguish verified
behavior from remaining work. Development is AI-assisted; passing tests cover
specific cases and do not certify production readiness.

## Run a small backend

The [task API guide](examples/todo-api/README.md) explains database setup,
environment variables, curl requests, and deployment considerations.

```sh
export DATABASE_URL='postgresql://app:your-password@127.0.0.1:5432/tasks?connect_timeout=5'
export API_TOKEN="$(openssl rand -hex 32)"
nextviper run examples/todo-api/app.nv
```

It serves `/health` and authenticated CRUD routes at `/tasks`. PostgreSQL stores
the tasks across app restarts. This is a single-user example; it has no account
system or per-user permissions. The server processes requests sequentially. A
small hosted deployment needs a TLS reverse proxy, a restricted service account,
database backups, and workload-specific testing.

## Execution paths

- `run file.nv` or `run file.nv --tree`: the recommended interpreter, including
  HTTP, PostgreSQL, file, and process modules.
- `run file.nv --vm`: an experimental in-memory VM. It supports core arithmetic,
  collections, functions without lexical captures, and control flow. Module
  imports and captured outer locals are explicitly rejected. Bytecode operand
  and constant limits are reported rather than truncated.
- `build file.nv -o program`: generates standalone C++20 using tagged values and
  invokes a host compiler. Modules and syntax are a subset; unsupported features
  produce diagnostics. There is no general performance guarantee.
- `build --bytecode`: unavailable until persistent bytecode serialization and
  loading are implemented.
- `check`, `fmt`, `lint`, `test`, and `nextviper-lsp`: experimental tooling.
  Static checking is gradual and incomplete; formatting conservatively preserves
  lexical tokens and indentation.

The npm package is a launcher for an installed native toolchain. It does not
bundle or download the compiler. Set `NEXTVIPER_BIN` and `NEXTVIPER_LSP_BIN` to
explicit executable paths. See [npm usage](npm/README.md).

## Validation and trust

`make test` and CTest include language, HTTP, transport, and installation
regressions. Set `NEXTVIPER_TEST_DATABASE_URL` to a dedicated PostgreSQL test
database to exercise the actual task API, CRUD, authentication, and restart
persistence. Without it, the database tests report a skip. Both Linux CI jobs and
the release workflow supply a real PostgreSQL service.

```sh
NEXTVIPER_TEST_DATABASE_URL='postgresql://app:password@127.0.0.1:5432/test_tasks?connect_timeout=5' make test
python3 tests/native_verify/run_all.py
```

NextViper runs with the permissions of its process. It does not sandbox untrusted
code. Reference counting does not collect cycles. See [security](SECURITY.md),
[testing](TESTING.md), [hardening](HARDENING.md), and [performance](PERFORMANCE.md).
No general speed advantage over Python, Go, Rust, or C++ is claimed. Release
artifacts are prepared as draft prereleases for review.

Project: [nextviper.nuratix.com](https://nextviper.nuratix.com) ·
Developed by [Nuratix](https://www.nuratix.com) · team@nuratix.com
