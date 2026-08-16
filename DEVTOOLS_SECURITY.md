# NextViper Developer Tooling Security Architecture

NextViper adheres to strict security principles across its developer tools, compiler, package manager, and build pipelines.

---

## 1. Security Architecture Principles

### Zero Execution on Inspection
NextViper developer tools (`nextviper check`, `nextviper fmt`, `nextviper lint`, `nextviper doc`, `nextviper list`) **never execute arbitrary code** during static analysis or package inspection. All tools operate strictly on static AST tokens and metadata.

### Cryptographic Package Integrity
- Every published package archive is hashed using SHA-256.
- Manifests and tree hierarchies are cryptographically hashed before download and installation.
- Tampered or modified packages fail verification automatically (`NV205`).

### Path Traversal Prevention
Package archives and module imports cannot access files outside the declared workspace root. Relative paths attempting directory traversal (`../../etc/passwd`) are rejected during lexing and package extraction.

### Parameterized SQL Query Safety
The database standard library (`std.db`) strictly requires parameterized query formats (`$1, $2, ...`), isolating query structures from user inputs and preventing SQL injection vulnerabilities.

### Secure Password & Token Primitives
The cryptographic module (`std.crypto`) employs **PBKDF2-SHA256** with 10,000 iterations and distinct salts, **HMAC-SHA256**, and standard **RFC 7519 JWT** signatures with constant-time verification.
