# NextViper Website Production Deployment Guide

## 1. Deployment Specification

- **Repository**: `NextViperweb` (`https://github.com/nuratix/NextViperweb.git`)
- **Branch**: `main`
- **Framework**: Next.js 14.2.35 (React 18, App Router, TypeScript)
- **Deployment Platform**: Vercel / Docker Container
- **Production Domain**: `https://nextviper.nuratix.com`
- **Build Command**: `npm run build` (`npm run db:migrate && next build`)
- **Node Engine**: `>= 18.0.0`

---

## 2. Production Environment Configuration

The following environment variables are supported for full registry and web portal operations:

| Variable | Description | Default / Fallback |
| :--- | :--- | :--- |
| `POSTGRES_URL` / `DATABASE_URL` | PostgreSQL connection string (Neon / Supabase / standard) | Local `.storage/registry_db.json` fallback |
| `NEXT_PUBLIC_APP_URL` | Production website base URL | `https://nextviper.nuratix.com` |
| `NEXTVIPER_REGISTRY_URL` | Package registry API base URL | `https://nextviper.nuratix.com/api` |
| `JWT_SECRET` | Secret for CLI API tokens and session cookies | Cryptographically generated HMAC key |
| `STORAGE_BACKEND` | Package tarball storage backend | Local disk / S3-compatible object storage |

---

## 3. Verified Production Routes

All production routes are verified with 0 broken links (verified via `npm run test:links`):

- `GET /` — Homepage with interactive code playground and feature highlights.
- `GET /install` — Official installation methods (npm, universal installer, source).
- `GET /download` — Prebuilt binaries, source tarballs, and SHA-256 checksums.
- `GET /releases` — Complete changelog and version history.
- `GET /license` — Official Apache-2.0 open-source license text.
- `GET /contributing` — Step-by-step developer contribution guidelines.
- `GET /security` — Vulnerability disclosure policy and security model.
- `GET /docs` — Documentation hub (22 topics covering language, stdlib, AI, tensors, GPU, compiler).
- `GET /docs/errors` — Diagnostic error code index (16 error codes with solutions).
- `GET /packages` — Package registry browser with category filters.
- `GET /search` — Full-text package search engine.
- `GET /install.sh` — Universal POSIX shell installer script.

---

## 4. Operational Status

- **Build Status**: `PASS` (78 static & dynamic pages compiled successfully).
- **Vitest Suite**: `PASS` (6 test files, 25 unit/integration tests passing).
- **Link Integrity**: `PASS` (0 broken links across 36 static routes and 38 dynamic documentation topics).
- **Distribution Integrity**: `PASS` (Binary archives and source tarball match exact SHA-256 hashes).
