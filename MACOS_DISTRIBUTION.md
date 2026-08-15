# macOS Distribution & Homebrew Architecture

**Platforms**: macOS 12 Monterey, macOS 13 Ventura, macOS 14 Sonoma, macOS 15 Sequoia  
**Architectures**: Apple Silicon (`arm64` / `aarch64`), Intel (`x86_64`)

---

## 1. Supported Distribution Channels

### 1.1 Official POSIX Installer (`install.sh`)
- **Status**: **IMPLEMENTED & ACTIVE**
- **Command**:
  ```bash
  curl -fsSL https://nextviper.nuratix.com/install.sh | sh
  ```
- **Behavior**: Detects macOS (`Darwin`), determines whether running on M1/M2/M3 Apple Silicon or Intel, installs binaries to `~/.nextviper/bin`, and adds to `~/.zshrc` / `~/.bash_profile`.

### 1.2 Homebrew Formula (`brew`)
- **Status**: PLANNED (Formula specification defined)
- **Target Command**:
  ```bash
  brew install nuratix/tap/nextviper
  ```

---

## 2. Homebrew Formula Specification

```ruby
class Nextviper < Formula
  desc "Programming language for Data, Tensor, AI, and GPU compute"
  homepage "https://nextviper.nuratix.com"
  version "1.0.0"
  license "Apache-2.0"

  on_macos do
    if Hardware::CPU.arm?
      url "https://github.com/nuratix/nextviper/releases/download/v1.0.0/nextviper-v1.0.0-darwin-arm64.tar.gz"
      sha256 "<DARWIN_ARM64_SHA256>"
    else
      url "https://github.com/nuratix/nextviper/releases/download/v1.0.0/nextviper-v1.0.0-darwin-x86_64.tar.gz"
      sha256 "<DARWIN_X86_64_SHA256>"
    end
  end

  on_linux do
    if Hardware::CPU.arm?
      url "https://github.com/nuratix/nextviper/releases/download/v1.0.0/nextviper-v1.0.0-linux-arm64.tar.gz"
      sha256 "<LINUX_ARM64_SHA256>"
    else
      url "https://github.com/nuratix/nextviper/releases/download/v1.0.0/nextviper-v1.0.0-linux-x86_64.tar.gz"
      sha256 "<LINUX_X86_64_SHA256>"
    end
  end

  def install
    bin.install "nextviper"
    bin.install "nextviper-lsp"
  end

  test do
    assert_match "NextViper 1.0.0", shell_output("#{bin}/nextviper --version")
  end
end
```
