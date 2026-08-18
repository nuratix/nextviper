# NextViper Documentation Architecture & Source of Truth

## 1. Authoritative Source of Truth Definition

To ensure consistency and avoid contradictory documentation across the NextViper ecosystem, the authority is strictly demarcated across the two official repositories:

```
+-------------------------------------------------------------------------+
|                MAIN NEXTVIPER COMPILER REPOSITORY                      |
|                  (https://github.com/nuratix/nextviper)                 |
|                                                                         |
|  * Authoritative for:                                                   |
|    - Language grammar, AST, and semantic specification                  |
|    - Standard library modules (std.http, std.fs, std.db, etc.)          |
|    - Native AOT compiler architecture & C emitter pipeline              |
|    - Bytecode VM & Interpreter runtime mechanics                        |
|    - Tensor, autograd, AI layers, and Vulkan GPU backend                |
|    - CLI flags and subcommands (run, build, check, fmt, lint, test, lsp)|
|    - Diagnostic error codes and messages                                |
+-------------------------------------------------------------------------+
                                    |
                                    v (Automated / Synchronized Content)
+-------------------------------------------------------------------------+
|                     NEXTVIPERWEB REPOSITORY                             |
|                (https://github.com/nuratix/NextViperweb)                |
|                     (https://nextviper.nuratix.com)                     |
|                                                                         |
|  * Authoritative for:                                                   |
|    - Developer portal UI / UX components & visual design tokens         |
|    - Interactive tutorial documentation & code highlighting             |
|    - Registry REST API endpoints (/api/packages, /api/auth, /api/tokens)|
|    - Web authentication, 2FA, FIDO2 biometrics, and session security    |
|    - Package browsing, search ranking, and download CDN distribution    |
|    - Universal shell installer script hosting (/install.sh)             |
+-------------------------------------------------------------------------+
```

---

## 2. Synchronization Workflow

When a language feature, standard library function, or compiler capability is implemented or modified:

1. **Compiler Repository First**:
   - Implementation is completed and verified with regression tests in `nextviper/tests/`.
   - Markdown references in `nextviper/docs/` and `IMPLEMENTATION_STATUS.md` are updated to reflect exact capabilities.

2. **Web Portal Synchronization**:
   - Technical specifications and code examples are updated in `NextViperweb/src/lib/docs-data.ts`.
   - Error code diagnostics are synchronized in `NextViperweb/src/lib/error-docs.ts`.
   - Distribution artifacts and checksums are updated in `NextViperweb/src/lib/releases.ts`.

3. **Validation & Deployment**:
   - Run `npm test` and `npm run test:links` in `NextViperweb`.
   - Build static artifacts via `npm run build`.
   - Deploy to production at `https://nextviper.nuratix.com`.

---

## 3. Official Distribution Channels

NextViper is distributed exclusively through three official channels:

1. **npm Global Package**: `npm install -g nextviper` (Executable `nextviper` CLI and `nextviper-lsp`).
2. **GitHub Releases & Source**: `https://github.com/nuratix/nextviper` (Source tarballs and GitHub Actions CI artifacts).
3. **Official Website Portal**: `https://nextviper.nuratix.com` (Universal installer `/install.sh`, release tarballs, docs, and registry).

*(Note: NextViper is not distributed through Android Termux package repositories).*
