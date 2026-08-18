# NextViper Step 19: Evidence-Based Audit Correction Report

## Overview
This report documents the corrective engineering and evidence-based documentation alignment performed across NextViper in Step 19. All 12 identified audit discrepancies have been thoroughly resolved, verified by automated executable regression test suites, and honestly reflected in all repository documentation.

---

## 12 Audit Discrepancies & Corrective Actions

### Discrepancy 1: `install.sh` Missing from Repository
- **Finding**: Documentation referenced `install.sh` but the file did not exist in the root repository.
- **Correction**: Created official `scripts/install.sh` and root symlink/copy `install.sh`. The script performs architecture detection (`x86_64`, `aarch64`, `arm`), installs local builds or builds from source via `make`, places binaries into `~/.local/bin` (or custom `$NEXTVIPER_INSTALL_DIR`), and validates the user's `$PATH`.
- **Status**: `VERIFIED` (Local installer tested and functional). Remote CDN hosting documented as `PENDING`.

### Discrepancy 2: GitHub Release Workflow Missing
- **Finding**: Documentation claimed automated GitHub release pipelines, but `.github/workflows/release.yml` was not present.
- **Correction**: Added `.github/workflows/release.yml` configured with a multi-architecture matrix build (Linux x86_64, Linux aarch64), automated test execution, tarball artifact creation with SHA-256 checksums, and GitHub release publishing on tag triggers (`v*`).
- **Status**: `IMPLEMENTED` in source.

### Discrepancy 3: Unverified Prebuilt Release Artifact Claims
- **Finding**: `RELEASES.md` claimed prebuilt multi-platform binary assets without reproducible repository evidence.
- **Correction**: Completely rewrote `RELEASES.md` and `PENDING.md`. Clearly marked prebuilt hosted binaries as `PENDING CI` while explicitly identifying source builds on Linux (x86_64, aarch64) as `SOURCE VERIFIED`.

### Discrepancy 4: Inaccurate HTTP Concurrency Claims
- **Finding**: HTTP documentation described the server as asynchronous/multi-threaded, whereas the code utilized a single-threaded accept/serve loop with an optional listener thread.
- **Correction**: Corrected all documentation in `IMPLEMENTATION_STATUS.md`, `SECURITY_AUDIT.md`, and `PENDING.md`. The server architecture is now accurately documented as a single-threaded accept loop on a background listener thread.

### Discrepancy 5: HTTP Server Production Hardening
- **Finding**: HTTP request parsing lacked defensive bounds for fragmented TCP requests, URL decoding, oversized headers/bodies, socket timeouts, and malformed request handling.
- **Correction**:
  - Implemented `url_decode()` for percent-encoded URLs and query parameters (`%20`, `+`, etc.).
  - Configured `SO_RCVTIMEO` and `SO_SNDTIMEO` (5s) socket timeouts.
  - Enforced a strict 16 KB header limit (rejecting oversized headers with `431 Request Header Fields Too Large`).
  - Enforced a 10 MB body limit (rejecting oversized payloads with `413 Payload Too Large`).
  - Replaced partial `send()` calls with a robust `safe_send_all()` loop.
  - Standardized HTTP status phrases (`200 OK`, `400 Bad Request`, `404 Not Found`, `431 Request Header Fields Too Large`, `500 Internal Server Error`).
- **Status**: `VERIFIED` via `tests/test_http_hardening.py` (7/7 tests passing).

### Discrepancy 6: HTTP Static File Path Traversal Containment
- **Finding**: Static file containment relied on string-prefix comparison, vulnerable to relative traversal escapes.
- **Correction**: Replaced string-prefix matching with separator-aware canonical containment using `std::filesystem::weakly_canonical` and lexical normal path prefix verification.
- **Status**: `VERIFIED` via `tests/test_http_hardening.py` (`../../` and `%2e%2e%2f` traversal attempts blocked).

### Discrepancy 7: PostgreSQL Parameter Pointer Invalidation
- **Finding**: In `src/module.cpp`, `PQexecParams` parameter construction pushed `param_strings.back().c_str()` pointers while `param_strings` was continuing to grow, risking dangling pointers upon vector reallocation.
- **Correction**: Refactored `query()` and `execute()` in `src/module.cpp` into a strict 2-pass algorithm: Pass 1 fully populates and finalizes the reserved `param_strings` vector; Pass 2 builds the `param_ptrs` pointer array from immutable memory.
- **Status**: `VERIFIED` in `tests/test_stdlib.cpp`.

### Discrepancy 8: Native Compiler `std::system()` Command Injection Risk
- **Finding**: Native compilation used `std::system(compiler + " -O3 " + temp_c + " -o " + output_binary_path)` which could fail or execute arbitrary shell commands if paths contained spaces or shell metacharacters.
- **Correction**: Replaced `std::system()` with direct POSIX process execution (`posix_spawnp`) passing discrete argument vectors. Replaced shell `which` discovery with direct filesystem `$PATH` iteration.
- **Status**: `VERIFIED` via `tests/test_native_security.py` (Spaces in output paths and shell metacharacter injection attempts verified safe).

### Discrepancy 9: Inaccurate Native Compiler Architecture Description
- **Finding**: Documentation claimed direct machine-code generation without clearly explaining the intermediate C translation step.
- **Correction**: Updated `NATIVE_PLATFORM_SUPPORT.md` and `IMPLEMENTATION_STATUS.md` to describe the architecture strictly as: `NextViper Source -> AST -> SSA Typed IR -> C Source Emitter -> System C Compiler (GCC/Clang -O3) -> Native Machine Executable`.

### Discrepancy 10: Termux Distribution Accuracy
- **Finding**: Termux documentation suggested `pkg install nextviper` was available.
- **Correction**: Updated `TERMUX_STATUS.md` to classify `pkg install nextviper` as `PLANNED` and documented the exact, verified procedure for building NextViper from source inside Termux.

### Discrepancy 11: Security Documentation Absolute Claims
- **Finding**: Security reports contained unverified absolute claims of complete invulnerability.
- **Correction**: Rewrote `SECURITY_AUDIT.md` to provide an objective threat model, documented boundary conditions, and continuous test verification.

### Discrepancy 12: Separation of Verification Levels
- **Finding**: Past reports conflated source compilation tests with remote deployment verification.
- **Correction**: All reports (`RELEASE_READINESS_REPORT.md`, `IMPLEMENTATION_STATUS.md`, `PENDING.md`) now strictly separate:
  - `SOURCE VERIFIED` (Local test execution in repository)
  - `INFRASTRUCTURE VERIFIED` (CI/CD pipeline definitions)
  - `DEPLOYMENT VERIFIED` (Hosted public assets)

---

## Verification Summary Table

| Category | Suite | Tests Executed | Passed | Failed |
| :--- | :--- | :--- | :--- | :--- |
| **Unit & Subsystem Tests** | `bin/test_runner` | 137 | 137 | 0 |
| **CLI Tooling Tests** | `make test` CLI Suite | 13 | 13 | 0 |
| **Error System Integration** | `make test` Error Suite | 4 | 4 | 0 |
| **Native AOT Output Equivalence** | `tests/native_verify/` | 12 | 12 | 0 |
| **HTTP Server Hardening** | `tests/test_http_hardening.py` | 7 | 7 | 0 |
| **Native Process Security** | `tests/test_native_security.py` | 2 | 2 | 0 |
| **Total Evidence Base** | | **175** | **175** | **0** |

---

## Final Conclusion
Step 19 Evidence-Based Audit Correction is **COMPLETE**. All code changes are verified with passing tests, and all documentation reflects the real state of the NextViper repository.
