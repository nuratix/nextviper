# NextViper Error Code Registry

NextViper uses standardized, stable, machine-readable error codes across the compiler, runtime, package manager, and CLI tooling.

Every error code corresponds to a real diagnostic condition and links to dedicated documentation at `https://nextviper.nuratix.com/docs/errors/<slug>`.

---

## Error Codes Reference

| Error Code | Slug | Title | Category | Description | Documentation URL |
| :--- | :--- | :--- | :--- | :--- | :--- |
| `NV1001` | `unknown-identifier` | Unknown Identifier | Compiler | Identifier, variable, or function is not declared in scope | [NV1001 Docs](https://nextviper.nuratix.com/docs/errors/unknown-identifier) |
| `NV1002` | `syntax-error` | Syntax Error | Compiler | Source code violates language syntax rules | [NV1002 Docs](https://nextviper.nuratix.com/docs/errors/syntax-error) |
| `NV1003` | `type-mismatch` | Type Mismatch | Compiler | Value type incompatible with target annotation/operand | [NV1003 Docs](https://nextviper.nuratix.com/docs/errors/type-mismatch) |
| `NV1004` | `invalid-function-call` | Invalid Function Call | Compiler | Incorrect argument count or signature mismatch | [NV1004 Docs](https://nextviper.nuratix.com/docs/errors/invalid-function-call) |
| `NV1005` | `module-not-found` | Module Not Found | Compiler | Imported module or file cannot be resolved | [NV1005 Docs](https://nextviper.nuratix.com/docs/errors/module-not-found) |
| `NV2001` | `package-not-found` | Package Not Found | Package Manager | Package is missing from registry or not installed | [NV2001 Docs](https://nextviper.nuratix.com/docs/errors/package-not-found) |
| `NV2002` | `file-not-found` | File Not Found | CLI | Target file cannot be opened or does not exist | [NV2002 Docs](https://nextviper.nuratix.com/docs/errors/file-not-found) |
| `NV2003` | `invalid-argument` | Invalid CLI Argument | CLI | Unrecognized command or flag option | [NV2003 Docs](https://nextviper.nuratix.com/docs/errors/invalid-argument) |
| `NV3001` | `dependency-resolution` | Dependency Resolution | Package Manager | Failed to solve compatible SemVer version constraints | [NV3001 Docs](https://nextviper.nuratix.com/docs/errors/dependency-resolution) |
| `NV3002` | `package-integrity` | Package Integrity Checksum | Package Manager | Archive SHA-256 hash mismatch with lockfile | [NV3002 Docs](https://nextviper.nuratix.com/docs/errors/package-integrity) |
| `NV4001` | `division-by-zero` | Division by Zero | Runtime | Division or modulo by zero at runtime | [NV4001 Docs](https://nextviper.nuratix.com/docs/errors/division-by-zero) |
| `NV4002` | `index-out-of-bounds` | Index Out of Bounds | Runtime | Subscript index outside valid collection range | [NV4002 Docs](https://nextviper.nuratix.com/docs/errors/index-out-of-bounds) |
| `NV4003` | `key-not-found` | Key Not Found | Runtime | Key does not exist in target dictionary map | [NV4003 Docs](https://nextviper.nuratix.com/docs/errors/key-not-found) |
| `NV4004` | `null-reference` | Null Reference | Runtime | Member access or operation performed on null value | [NV4004 Docs](https://nextviper.nuratix.com/docs/errors/null-reference) |
| `NV4005` | `file-io-error` | File I/O Error | Runtime | Runtime filesystem read/write operation failed | [NV4005 Docs](https://nextviper.nuratix.com/docs/errors/file-io-error) |
| `NV5001` | `compiler-error` | Internal Compiler Error | System | Native code generation or optimizer failure | [NV5001 Docs](https://nextviper.nuratix.com/docs/errors/compiler-error) |

---

## Machine-Readable JSON Format (`nextviper check --format=json`)

```json
[
  {
    "level": "error",
    "code": "NV1001",
    "message": "unknown identifier `calculate`",
    "file": "src/main.nv",
    "line": 4,
    "column": 12,
    "end_line": 4,
    "end_column": 21,
    "hint": "declare `calculate` before using it",
    "documentation": "https://nextviper.nuratix.com/docs/errors/unknown-identifier"
  }
]
```
