# NextViper Distribution & CI/CD Secrets Guide

**Notice**: This document defines the required CI/CD secret variables and their intended scopes. **Never commit actual secret values or credentials to Git repositories.**

---

## 1. Required External Credentials

| Secret Variable | Purpose | Scope / Permissions | Configuration Location | Status |
| :--- | :--- | :--- | :--- | :--- |
| `GITHUB_TOKEN` | GitHub Releases creation, tag management, and asset uploads | `contents: write`, `packages: write` | GitHub Actions (Auto-provided / Org Secret) | **ACTIVE** |
| `RELEASE_GPG_PRIVATE_KEY` | Digital signing of release manifests (`SHA256SUMS.sig`) | Subkey with `sign` capability | GitHub Actions Secret `GPG_RELEASE_KEY` | **CONFIGURED** |
| `RELEASE_GPG_PASSPHRASE` | Passphrase to unlock the release signing GPG key | Read-only in release workflow | GitHub Actions Secret `GPG_PASSPHRASE` | **CONFIGURED** |
| `REGISTRY_DATABASE_URL` | Production PostgreSQL connection string for package registry | Read/Write on registry schema | Vercel / Railway / Production Host | **ACTIVE** |
| `REGISTRY_JWT_SECRET` | Signing authorization tokens for package publishing | 256-bit HMAC secret | Vercel / Production Environment | **ACTIVE** |
| `WINGET_TOKEN` | Automated PR submission to `microsoft/winget-pkgs` | `public_repo` GitHub PAT | GitHub Actions Secret `WINGET_TOKEN` | **PENDING** |
| `HOMEBREW_TAP_TOKEN` | Automated formula update on `nuratix/homebrew-tap` | `repo` GitHub PAT | GitHub Actions Secret `HOMEBREW_TOKEN` | **PENDING** |
