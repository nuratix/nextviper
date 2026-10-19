# Build system

The source toolchain requires C++20, Vulkan/PostgreSQL development libraries and POSIX threading. See [README.md](README.md) for Linux prerequisites and Make/CMake commands.

`nextviper build file.nv -o program` produces a native executable by default. The pipeline parses and checks source, emits register IR, generates a standalone C++20 translation unit, and invokes a host C++ compiler. Normal/release native builds use `-O2`; `--debug` disables IR optimization and uses `-O0 -g`. No throughput guarantee is made.

`--bytecode` reports an error because artifact serialization/loading is unfinished. The VM still executes bytecode in memory through `run --vm`. Unsupported native modules and operations should fail with diagnostics rather than generate misleading artifacts.

The native executable uses the C++ standard runtime. Portability, deployment dependencies and unsupported library features must be checked on each target platform.
