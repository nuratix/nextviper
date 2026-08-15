# npm / Node.js Distribution Evaluation

**Status**: NOT RECOMMENDED / REJECTED FOR CORE COMPILER  
**Evaluation Target**: `npm install -g nextviper`

---

## 1. Technical Evaluation

NextViper was evaluated to determine whether `npm` is an appropriate distribution vector for the NextViper compiler, runtime, and language server.

### 1.1 Architecture Analysis
- NextViper is a native systems programming language written in standard C++20 with Vulkan GPU bindings.
- Node.js / npm is a JavaScript runtime package ecosystem.

### 1.2 Evaluation Findings
1. **Unnecessary Dependency Chain**: Requiring developers to install Node.js and npm solely to install a native systems compiler contradicts NextViper's zero-dependency philosophy.
2. **Post-Install Binary Download Risks**: Many npm CLI wrappers use `postinstall` lifecycle hooks to fetch external binary tarballs. This pattern frequently breaks in air-gapped CI environments, creates security attack vectors, and causes proxy failures.
3. **Dedicated VS Code Extension**: For developer tooling within JavaScript/TypeScript environments (e.g. VS Code), the NextViper extension is distributed directly as a standard `.vsix` package through the VS Code Marketplace and Open VSX Registry.

---

## 2. Decision

NextViper will **NOT** be distributed via `npm install -g nextviper`.

Developers on Linux and macOS should use the official POSIX installer (`curl -fsSL https://nextviper.nuratix.com/install.sh | sh`) or official GitHub Releases tarballs.
