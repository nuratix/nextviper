# Installing NextViper

NextViper supports multiple official installation methods for Linux, macOS, and POSIX environments.

---

## 1. Quick Install (Recommended for Linux & macOS)

Install the latest stable release via the official HTTPS installer script:

```bash
curl -fsSL https://nextviper.nuratix.com/install.sh | sh
```

### Install a Specific Version
To install a specific release version:
```bash
curl -fsSL https://nextviper.nuratix.com/install.sh | NEXTVIPER_VERSION=1.0.0 sh
```

### Non-Interactive / Custom Directory
```bash
curl -fsSL https://nextviper.nuratix.com/install.sh | NEXTVIPER_INSTALL_DIR=/opt/nextviper sh
```

---

## 2. Direct Release Downloads (Pre-Compiled Tarballs)

You can download pre-built release archives directly from the official [NextViper Download Hub](https://nextviper.nuratix.com/download):

1. Download the archive for your architecture:
   - **Linux x86_64**: [`nextviper-v1.0.0-linux-x86_64.tar.gz`](https://nextviper.nuratix.com/downloads/nextviper-v1.0.0-linux-x86_64.tar.gz)
   - **Linux ARM64**: [`nextviper-v1.0.0-linux-arm64.tar.gz`](https://nextviper.nuratix.com/downloads/nextviper-v1.0.0-linux-arm64.tar.gz)
   - **macOS Apple Silicon**: [`nextviper-v1.0.0-darwin-arm64.tar.gz`](https://nextviper.nuratix.com/downloads/nextviper-v1.0.0-darwin-arm64.tar.gz)
   - **macOS Intel**: [`nextviper-v1.0.0-darwin-x86_64.tar.gz`](https://nextviper.nuratix.com/downloads/nextviper-v1.0.0-darwin-x86_64.tar.gz)
2. Verify checksum:
   ```bash
   sha256sum --check --ignore-missing SHA256SUMS
   ```
3. Extract and link to PATH:
   ```bash
   tar -xzf nextviper-v1.0.0-linux-x86_64.tar.gz
   sudo mv nextviper /usr/local/bin/
   sudo mv nextviper-lsp /usr/local/bin/
   ```

---

## 3. Building from Source

Building from source ensures maximum optimization for your CPU and GPU architecture.

### System Requirements
- GCC 11+ or Clang 13+ with C++20 support
- GNU Make or CMake 3.20+
- Vulkan SDK headers and loader (for GPU compute acceleration)
- `pthread` library

### Build Steps
```bash
git clone https://github.com/nuratix/nextviper.git
cd nextviper
make -j$(nproc)
make test
sudo cp bin/nextviper bin/nextviper-lsp /usr/local/bin/
```

---

## 4. Verification

After installation, verify that the NextViper toolchain is working:

```bash
nextviper --version
nextviper-lsp --help
nextviper eval "print('Hello, NextViper!')"
```
