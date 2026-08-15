# NextViper Implementation & Ecosystem Status

This document provides a transparent overview of implemented features, pending roadmap items, and blocked external requirements.

---

## 1. IMPLEMENTED & ACTIVE

- **Core Language & C++20 Toolchain**: Full lexer, AST, parser, type checker, interpreter, and native compiler.
- **Data & AI Subsystems**: Columnar DataFrame, autograd Tensor engine, neural network layers, loss functions, and optimizers.
- **Vulkan GPU Compute Backend**: Hardware device detection, host/device memory transfers, GEMM matrix multiplication, and shader execution.
- **Standard Library (`std`)**: 14 standard modules (`fs`, `math`, `collections`, `json`, `csv`, `crypto`, `time`, `process`, `regex`, etc.).
- **Package Manager CLI & Registry**: `nextviper.toml`, `nextviper.lock`, SHA-256 tree hashing, SemVer resolution.
- **Developer Tooling**: Standalone `nextviper-lsp` language server, VS Code syntax and LSP extension, `nextviper fmt`, `nextviper check`, `nextviper test`, `nextviper info`, `nextviper run --watch`.
- **Stable Error Reference System**: 16 error codes (`NV1001`–`NV5001`) with live docs on `https://nextviper.nuratix.com/docs/errors/<slug>`.
- **Official POSIX Installer (`install.sh`)**: HTTPS installer supporting OS and CPU architecture detection, checksum validation, and automated PATH configuration.
- **Website Legal & Distribution Hubs**: `/install`, `/download`, `/releases`, `/license`, `/contributing`, `/security`, `/privacy`, `/terms`.

---

## 2. PARTIALLY IMPLEMENTED

- **Debian / Ubuntu Native Package (`.deb`)**: Package control files defined; awaiting launchpad PPA repository setup.
- **Nightly CI Matrix Builds**: GitHub Actions workflow defined for multi-arch builds.

---

## 3. PLANNED

- **Homebrew Core / Tap Formula**: Formula specification defined in `MACOS_DISTRIBUTION.md`; community PR to be submitted following v1.0.0 tag.
- **Windows WinGet Manifest**: Package specification defined in `WINDOWS_DISTRIBUTION.md`; PR to `microsoft/winget-pkgs` planned.
- **Python Extension Module (`nextviper-py`)**: C-API bindings on PyPI for zero-copy tensor interoperability with PyTorch and NumPy.

---

## 4. BLOCKED BY EXTERNAL REQUIREMENT

- **Microsoft Store Distribution**: Blocked pending corporate Nuratix LLC Microsoft Partner Center account verification and app identity certificates.
- **Apple Developer ID Gatekeeper Notarization**: Required for unprompted macOS Gatekeeper binary distribution outside Homebrew.
