# Security status and reporting

NextViper is experimental. It has not undergone a comprehensive independent security audit and is not a sandbox for untrusted source or packages. Programs inherit the host process's filesystem, process, and network permissions.

Please report suspected vulnerabilities privately through the repository owner's GitHub security reporting channel if available. Avoid publishing credentials or private application data in issues.

## Current defenses and limits

- HTTP client calls use separate subprocess arguments instead of a shell. Methods, header names/values, and HTTP/HTTPS URL schemes are validated. curl timeouts and a response-size limit apply to this client path.
- Collection printing detects cycles and limits nesting. DataFrame wrappers validate column/row argument shapes.
- The REPL retains declarations for as long as its session can call them.
- Native values carry explicit tags instead of treating large integers as pointers. Native collection access performs type/bounds checks.
- Some parser, call-stack and allocation limits exist. They do not bound every execution path, memory allocation or external operation.

Untrusted URLs still require an application-level SSRF policy. Process execution APIs intentionally execute commands. Package/archive handling, native resource management, reference cycles, runtime arithmetic, and concurrency need further review. Do not interpret these targeted fixes as a general security guarantee.

See [CURRENT_STATUS.md](CURRENT_STATUS.md), [HARDENING.md](HARDENING.md), and the regression tests for the supported scope. Sanitizers can help find bugs but do not prove their absence.
