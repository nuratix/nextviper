# NextViper Release Policy & Governance

**Release Authority**: Nuratix LLC Release Engineering  
**Versioning Standard**: Semantic Versioning 2.0.0 (`MAJOR.MINOR.PATCH`)

---

## 1. Release Authority & Ownership

While NextViper is an open-source project with active community contributions, **official release artifacts, binaries, and version tags are strictly created and signed by authorized maintainers from Nuratix LLC**.

This policy guarantees:
- **Binary Integrity**: Every release binary is built in a verified, cleanroom CI environment.
- **Supply Chain Security**: No unauthorized binaries or packages can be distributed under the official NextViper name.
- **Strict Quality Control**: All releases must pass 100% of the automated unit, integration, and security test suites.

---

## 2. Versioning Semantics

NextViper strictly adheres to Semantic Versioning (`vMAJOR.MINOR.PATCH`):

- **MAJOR (`x.0.0`)**: Incompatible API changes, fundamental language grammar updates, or breaking standard library modifications.
- **MINOR (`0.x.0`)**: Backwards-compatible new features, additional standard library modules, performance optimizations, or new tooling subcommands.
- **PATCH (`0.0.x`)**: Backwards-compatible bug fixes, security patches, diagnostic clarity improvements, or documentation updates.

---

## 3. Support Lifecycle & Cadence

| Version Tier | Release Cadence | Support Window | Maintenance |
| :--- | :--- | :--- | :--- |
| **Stable (Latest)** | Every 6–8 weeks | Active development | Features + Bug fixes |
| **LTS (Long Term Support)** | Annual | 18 months | Security patches + Critical fixes |
| **Nightly / Development** | Automated on `main` | Ephemeral | Experimental features |
