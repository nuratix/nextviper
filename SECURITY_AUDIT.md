# NextViper Security Audit & Hardening Report

## Overview
This security assessment documents the hardened surfaces, mitigations, and known boundaries of NextViper 1.0.0. In accordance with honest engineering practices, absolute claims of total invulnerability are avoided in favor of rigorous threat analysis and executable regression testing.

---

## Hardened Subsystems & Mitigations

### 1. HTTP Server Hardening (`std.http`)
- **Path Traversal Containment**: Static file serving uses canonical lexical normalization (`std::filesystem::weakly_canonical` with separator-aware prefix verification). Requests attempting relative traversal (`../../` or URL-encoded `%2e%2e%2f`) are strictly contained within the designated root directory and cannot access parent filesystem paths.
- **Resource Exhaustion Protection**:
  - Max Header Size: 16 KB (Requests with unclosed or oversized headers trigger `431 Request Header Fields Too Large` or immediate connection termination).
  - Max Body Size: 10 MB (Requests exceeding content bounds trigger `413 Payload Too Large`).
- **Timeout Management**: Configured socket read/write timeouts (`SO_RCVTIMEO` / `SO_SNDTIMEO`) prevent slowloris connection starvation.
- **Malformed Protocol Handling**: Unrecognized request methods, empty routes, and corrupted HTTP lines return standard `400 Bad Request` responses.

### 2. Native Compiler Process Execution Safety
- **Shell Injection Immunity**: The native compiler replaces `std::system()` shell execution with direct POSIX process execution (`posix_spawnp`). Compiler arguments are passed as discrete pointers in a vector, rendering paths with spaces, semicolons, backticks, or shell metacharacters immune to command injection.
- **Binary Output Sandboxing**: Temporary `.c` source files and intermediate compilation artifacts are strictly removed following compilation.

### 3. Database Driver Memory Safety (`std.db.postgres`)
- **Stable Parameter Buffers**: Fixed a pointer invalidation vulnerability where `param_ptrs` stored `c_str()` pointers into a resizing vector. The driver now enforces a strict 2-pass allocation pattern: all string values are fully reserved and constructed in pass 1 before pointers are indexed in pass 2, guaranteeing pointer validity during `PQexecParams`.
- **SQL Injection Prevention**: All queries supporting parameters use prepared parameter substitution (`$1, $2, ...`) via PostgreSQL's native binary protocol rather than string concatenation.

---

## Remaining Threat Models & Recommendations

| Surface | Potential Risk | Mitigation / Best Practice |
| :--- | :--- | :--- |
| **HTTP Concurrency** | Single-threaded accept loop can be delayed by long-running synchronous route handlers. | Deploy NextViper behind a reverse proxy (Nginx / Envoy) for high-traffic production workloads. |
| **Vulkan GPU Kernels** | Malformed compute shaders could trigger device-lost errors. | Run compute tasks on dedicated GPU queues with device error handling. |
| **Package Dependency Supply Chain** | Malicious third-party package dependencies. | NextViper uses cryptographic tree hashing (SHA-256) in `nextviper.lock` to ensure build reproducibility. |

---

## Automated Security Regression Tests
Security assertions are verified continuously in the repository via:
- `tests/test_http_hardening.py`: Validates traversal blocking, encoded traversal rejection, oversized header limits, and TCP fragmentation.
- `tests/test_native_security.py`: Validates compilation of output paths containing spaces and shell metacharacters without arbitrary code execution.
