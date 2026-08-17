# NextViper Termux & Android Environment Status

## 1. Classification: EXPERIMENTAL

Termux (Android) is classified as an **EXPERIMENTAL** platform for NextViper compilation and native code generation.

---

## 2. Root Cause Analysis of Termux Build Failures

When building NextViper from source on Android devices running Termux, users may encounter sudden termination messages such as:
```
g++: fatal error: Killed signal terminated program cc1plus
make: *** [Makefile:45: build/module.o] Error 137
[Process completed (signal 9) - press Enter]
```

### Technical Root Causes:
1. **Signal 9 (SIGKILL) - Low Memory Killer (LMK)**:
   - NextViper's full source code (including standard library `module.cpp`, autograd engine `ai_subsystem.cpp`, and native C emitter `native_compiler.cpp`) comprises modern C++20 templates and header inclusions (`<filesystem>`, `<chrono>`, `<regex>`, `<vulkan/vulkan.h>`, `<libpq-fe.h>`).
   - Running `make` or `g++` without `-j1` flags causes memory usage to exceed Android's per-process cgroup memory limit (typically 512MB - 1.5GB on mobile devices), triggering the Linux kernel Out-Of-Memory (OOM) killer.

2. **Android 12+ Phantom Process Killer**:
   - Starting with Android 12, Google introduced the Phantom Process Killer (PPK), which strictly limits background processes spawned by user apps (such as sub-processes of `make`, `g++`, `as`, `ld`) to a total maximum of 32 concurrent processes across the entire Android user space.
   - When parallel compilation spawns multiple sub-processes, Android terminates the Termux session with `SIGKILL (signal 9)`.

---

## 3. Recommended Build Procedure on Termux

To successfully build NextViper on Android / Termux:

### Step 1: Install Single-Threaded Dependencies
```bash
pkg update && pkg upgrade -y
pkg install clang make libvulkan-dev -y
```

### Step 2: Build with Serial Execution (`-j1`)
```bash
# Do NOT run make -j4 or make -j8 on Termux. Run strictly with single core:
make clean
make -j1
```

### Step 3: Run Verification
```bash
./bin/nextviper --version
./bin/nextviper run examples/fibonacci.nv
```

---

## 4. Minimum System Requirements for Termux
- **Android OS**: Android 8.0+ (Oreo or newer)
- **Architecture**: `aarch64` (ARM64)
- **RAM**: Minimum 4GB physical RAM (with at least 2GB free before starting build)
- **Storage**: 1.5GB free internal storage
- **Swap / zRAM**: Recommended 2GB zRAM enabled for heavy template compilation units.
