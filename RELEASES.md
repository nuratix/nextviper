# NextViper Release History & Changelog

All official releases of NextViper follow [Semantic Versioning](https://semver.org/).

---

## [1.0.0] — 2026-08-15 (Initial Production Release)

The initial stable release of the NextViper programming language, native compiler, standard library, Vulkan GPU subsystem, and developer tooling.

### Highlights
- **Core Language & Type System**: Static and inferred typing, immutable `let` / mutable `let mut`, pattern matching, first-class functions, pipeline operator (`|>`), and zero-overhead closures.
- **Native Compiler & Backend**: x86-64 machine code generation, IR constant folding, dead-code elimination, and direct executable compilation.
- **Data Subsystem**: Vectorized `DataArray`, columnar `DataFrame`, automated CSV loading, preprocessing, filtering, and dataset splitting.
- **Tensor & AI Engine**: N-dimensional autograd tensors, forward/backward automatic differentiation, neural network layers (Dense, Dropout, Activations), loss functions, and optimizers (SGD, Adam).
- **Vulkan GPU Compute**: Hardware GPU device discovery, host-to-device memory transfers, parallel element-wise arithmetic, and high-performance GEMM matrix multiplication.
- **Standard Library (`std`)**: High-performance modules for `fs`, `path`, `string`, `collections`, `math`, `json`, `csv`, `time`, `process`, `crypto`, `regex`, `random`, and `concurrency`.
- **Package Manager & Registry**: Manifest (`nextviper.toml`), deterministic lockfile (`nextviper.lock`), cryptographic tree hashing, SemVer solver, and package publishing CLI.
- **Developer Tooling & LSP**: Standalone `nextviper-lsp` daemon (LSP 3.17), VS Code extension, in-place code formatter (`nextviper fmt`), project validator (`nextviper check`), interactive REPL, and benchmark suite.
- **Real Error Reference**: 16 stable error codes (`NV1001`–`NV5001`) with machine-readable JSON output and verified documentation URLs.

### Supported Platforms & Release Binaries
- `nextviper-v1.0.0-linux-x86_64.tar.gz`
- `nextviper-v1.0.0-linux-arm64.tar.gz`
- `nextviper-v1.0.0-darwin-arm64.tar.gz`
- `nextviper-v1.0.0-darwin-x86_64.tar.gz`
- `nextviper-v1.0.0-src.tar.gz`
