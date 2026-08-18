# NextViper Pending Infrastructure & Verification Matrix

This document tracks items that are planned, partially implemented, or awaiting external infrastructure deployment. NextViper policy strictly requires documenting unsupported claims as **PENDING** rather than claiming operational status without evidence.

---

## Pending Infrastructure Items

| Item | Category | Current Status | Blocker / Requirement |
| :--- | :--- | :--- | :--- |
| **Remote Installer CDN Hosting** | Distribution | `PLANNED` | `curl -fsSL https://nextviper.nuratix.com/install.sh \| bash` requires active public CDN deployment and DNS routing. Use `./scripts/install.sh` for local installation. |
| **Termux Official Repository Package** | Distribution | `PLANNED` | `pkg install nextviper` requires upstream submission to `termux/termux-packages`. Build from source using `clang` and `make` on Termux. |
| **Precompiled Multi-Platform Binaries** | Distribution | `PLANNED` | Automated builds for Windows (`x86_64-pc-windows-msvc`) and macOS (`aarch64-apple-darwin`, `x86_64-apple-darwin`) will be published via GitHub Actions when tags are pushed. |
| **Direct x86-64 Machine-Code JIT Backend** | Compiler | `PLANNED` | Current native compilation translates `NextViper IR -> C emitter -> system C compiler -> binary`. Direct LLVM IR / binary machine-code generation is planned for a future major release. |
| **Multi-Threaded HTTP Server Worker Pool** | Runtime | `PLANNED` | The current HTTP server utilizes a robust single-threaded accept/serve loop (with background listener thread support). A thread-pool worker architecture for high-concurrency request dispatching is planned. |
| **Remote Package Registry Authentication** | Package Manager | `PLANNED` | Package publishing with API key authentication to `https://registry.nextviper.org` is designed; public registry backend hosting is currently in private preview. |

---

## Verification Level Guidelines
1. **Source Verified**: Backed by executable source code in the repository and validated by automated test suites.
2. **Infrastructure Verified**: Automated CI/CD pipelines executing on remote build runners.
3. **Deployment Verified**: Hosted public artifacts available for download and signature-checked by end users.
