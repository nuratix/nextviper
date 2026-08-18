# NextViper Android / Termux Support Status

## Distribution Status
- **Official Package Repository (`pkg install nextviper`)**: `PLANNED` (Requires upstream submission to the official Termux package repository).
- **Source Compilation on Termux**: `SOURCE VERIFIED` (Fully supported using Termux native development toolchains).

---

## Building NextViper in Termux

### Prerequisites
Run the following commands inside Termux to install build dependencies:
```bash
pkg update
pkg install clang make git libvulkan-dev
```

### Compiling NextViper
```bash
git clone https://github.com/nuratix/nextviper.git
cd nextviper
make -j$(nproc)
./scripts/install.sh
```

### Verifying Installation
```bash
nextviper --version
nextviper -e 'print("NextViper running on Android / Termux")'
```

---

## Hardware Acceleration Notes
- **CPU Backend**: Fully operational on ARM64 / ARMv7.
- **GPU Acceleration**: Available on devices with Vulkan-compatible GPU drivers (Adreno / Mali). If hardware Vulkan is unavailable in Termux, NextViper automatically falls back to CPU tensor computations.
