# NextViper Installer Security Architecture

**Script URL**: `https://nextviper.nuratix.com/install.sh`  
**Protocol**: Strict TLS 1.3 / HTTPS

---

## 1. Security Guarantees & Threat Model

The official NextViper POSIX installer (`install.sh`) is designed to adhere to strict security best practices:

1. **HTTPS-Only Downloads**: All network operations enforce TLS encryption (`curl -fsSL` / `wget --secure-protocol`).
2. **Cryptographic Checksum Verification**: Every downloaded archive is verified against its authoritative SHA-256 hash before extraction.
3. **No Arbitrary Remote Code Execution**: The installer only extracts vetted binary archives; it never executes secondary remote scripts.
4. **Least Privilege Operation**: Defaults to installing into the user's home directory (`~/.nextviper/bin`) without requiring root/sudo privileges.
5. **Path Traversal Protection**: Tarballs are extracted inside isolated temporary directories (`mktemp -d`) with safe permission masks (`umask 077`).
6. **Atomic Rollback & Cleanup**: Any network failure, checksum mismatch, or permission error triggers immediate cleanup of temporary files and exits with a non-zero code.

---

## 2. Integrity Verification Workflow

```mermaid
graph TD
    A[User runs curl install.sh | sh] --> B[Detect OS & CPU Architecture]
    B --> C[Fetch Release Metadata & SHA256SUMS via HTTPS]
    C --> D[Download Platform Archive to Temp Directory]
    D --> E[Compute SHA-256 and Compare with Manifest]
    E -->|Mismatch| F[Abort & Clean Temp Directory]
    E -->|Valid Match| G[Extract to ~/.nextviper/bin]
    G --> H[Configure Shell PATH in .profile / .bashrc]
    H --> I[Execute nextviper --version Verification]
```

---

## 3. Manual Inspection & Auditing

Users who prefer not to pipe `curl` into `sh` can inspect and verify the installer locally:

```bash
# 1. Download installer script
curl -fsSL https://nextviper.nuratix.com/install.sh -o install.sh

# 2. Audit script contents
cat install.sh

# 3. Execute with desired options
sh install.sh
```
