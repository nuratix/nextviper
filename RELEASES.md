# NextViper Release Notes & Artifact Distribution

## Version 1.0.0 (Release Candidate)

### Platform Verification Matrix

| Platform / Target | Verification Status | Artifact Type | Notes |
| :--- | :--- | :--- | :--- |
| **Linux (aarch64)** | `SOURCE VERIFIED` | Source Build (`make`) | Full test suite passed (137/137 unit, 13/13 CLI, 4/4 error tests). |
| **Linux (x86_64)** | `SOURCE VERIFIED` | Source Build (`make`) | Compatible with GCC 11+ and Clang 14+. |
| **Linux (Prebuilt Tarball)** | `PENDING CI` | `nextviper-linux-x86_64.tar.gz` | Built automatically via `.github/workflows/release.yml` on release tag. |
| **macOS (Apple Silicon / Intel)** | `PENDING CI` | `nextviper-darwin-*.tar.gz` | Source compilation supported via `clang++`; prebuilt binaries pending CI runner. |
| **Windows (x86_64)** | `PENDING CI` | `nextviper-windows-x86_64.zip` | MSVC / MinGW compatible; prebuilt zip pending CI runner. |
| **Android (Termux)** | `SOURCE VERIFIED` | Source Build (`make`) | Requires `pkg install clang make git libvulkan`. Upstream `pkg install` pending. |

---

## Installation Methods

### Method 1: Build & Install From Source (Verified)
```bash
git clone https://github.com/nuratix/nextviper.git
cd nextviper
make -j4
./scripts/install.sh
```

### Method 2: Local One-Liner (Local Repository)
```bash
./install.sh
```

### Method 3: Remote Curl Installation (Pending CDN Deployment)
> **Note**: Remote installation via `curl https://nextviper.nuratix.com/install.sh | bash` is currently `PENDING` public CDN infrastructure deployment.
