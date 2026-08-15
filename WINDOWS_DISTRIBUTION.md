# Windows Distribution Architecture & Strategy

**Status**: PLANNED (WinGet Community Repository Submission)  
**Target Command**: `winget install Nuratix.NextViper`

---

## 1. Distribution Channels on Windows

NextViper supports Windows 10/11 x64 systems through the following planned channels:

### 1.1 Windows Package Manager (`winget`)
- **Manifest Format**: YAML manifest submitted to [microsoft/winget-pkgs](https://github.com/microsoft/winget-pkgs).
- **Package Identifier**: `Nuratix.NextViper`
- **Installer Type**: Standalone `.zip` archive containing `nextviper.exe` and `nextviper-lsp.exe` with portable command registration.

### 1.2 Microsoft Store
- **Status**: Blocked pending corporate Nuratix LLC Microsoft Partner Center account verification and app identity certificates.
- **Tracking**: Documented in `PENDING.md`.

---

## 2. Windows WinGet Manifest Specification

```yaml
# yaml-language-server: $schema=https://aka.ms/winget-manifest.version.1.6.0.schema.json
PackageIdentifier: Nuratix.NextViper
PackageVersion: 1.0.0
DefaultLocale: en-US
Publisher: Nuratix LLC
PublisherUrl: https://nuratix.com
PublisherSupportUrl: https://nextviper.nuratix.com/docs
PackageName: NextViper Programming Language
PackageUrl: https://nextviper.nuratix.com
License: Apache-2.0
LicenseUrl: https://nextviper.nuratix.com/license
ShortDescription: High-performance programming language for Data, Tensor, AI, and GPU compute.
Tags:
  - compiler
  - ai
  - tensor
  - gpu
  - vulkan
Installers:
  - Architecture: x64
    InstallerType: zip
    InstallerUrl: https://github.com/nuratix/nextviper/releases/download/v1.0.0/nextviper-v1.0.0-windows-x86_64.zip
    InstallerSha256: <SHA256_HASH>
    NestedInstallerType: portable
    NestedInstallerFiles:
      - RelativeFilePath: nextviper.exe
        PortableCommandAlias: nextviper
      - RelativeFilePath: nextviper-lsp.exe
        PortableCommandAlias: nextviper-lsp
ManifestType: singleton
ManifestVersion: 1.6.0
```
