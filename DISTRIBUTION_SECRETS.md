# NextViper Distribution & CI/CD Secrets Reference

This document catalogs the required environment variable secret names for release distribution and deployment automation.

> [!IMPORTANT]
> Never commit secret values or private credentials to source control. Configure these variable names in GitHub Actions Repository Secrets or server environment variables.

---

## 1. GitHub Releases & CI/CD Automation

| Variable Name | Purpose | Configuration Scope |
| :--- | :--- | :--- |
| `GITHUB_TOKEN` | Automated GitHub Release creation and binary asset uploads | Automatically provided by GitHub Actions runner |

---

## 2. npm Registry Distribution

| Variable Name | Purpose | Configuration Scope |
| :--- | :--- | :--- |
| `NPM_AUTH_TOKEN` | Publishing `nextviper` CLI distribution package to npm registry | GitHub Actions Secrets / Maintainer workstation |

---

## 3. Web Registry & PostgreSQL Database

| Variable Name | Purpose | Configuration Scope |
| :--- | :--- | :--- |
| `DATABASE_URL` / `POSTGRES_URL` | PostgreSQL connection string for live registry migrations and queries | Production Web Server / Vercel Environment |
| `JWT_SECRET` | Secret key for generating and validating CLI API tokens | Production Web Server Environment |
| `STORAGE_BACKEND` | Storage configuration for package tarball distribution | Production Web Server Environment |
