# NextViper Android / Termux Support Status

## Distribution Status
- **Official Package Repository (`pkg install nextviper`)**: `NOT_PLANNED` (NextViper is not distributed through Termux package repositories. Official channels are npm, GitHub releases/source, and https://nextviper.nuratix.com).
- **Source Compilation on Termux**: `SOURCE VERIFIED` (Technical manual source compilation is supported using Termux development toolchains `clang` and `make`).

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
