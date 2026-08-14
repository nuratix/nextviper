# NextViper Cross-Platform Build & Installation Guide (v1.0.0)

NextViper is written in standard modern C++20 with zero heavy third-party dependencies, making it portable and easy to build across Linux, macOS, and Windows.

---

## 1. System Requirements & Prerequisites

- **C++ Compiler**: A compiler with C++20 support:
  - GCC 11.0 or newer (Linux / MinGW)
  - Clang 13.0 or newer (Linux / macOS)
  - MSVC 2019 (v16.10+) or MSVC 2022 (Windows)
- **Build Systems**: GNU Make (`make`) or CMake (`cmake >= 3.16`)
- **POSIX Threads**: `pthread` (included with standard C++ toolchains)

---

## 2. Building on Linux (Ubuntu, Debian, Fedora, Arch)

### 2.1 Installing Dependencies

```bash
# Ubuntu / Debian
sudo apt update
sudo apt install -y build-essential cmake git

# Fedora / RHEL
sudo dnf groupinstall -y "Development Tools"
sudo dnf install -y gcc-c++ cmake

# Arch Linux
sudo pacman -S base-devel cmake git
```

### 2.2 Compiling with GNU Make

```bash
git clone https://github.com/nuratix/nextviper.git
cd nextviper

# Build compiler, VM, and CLI binary
make -j$(nproc)

# Run full test suite (unit + fuzz + CLI integration)
make test

# Install binary to system path
sudo cp bin/nextviper /usr/local/bin/
```

### 2.3 Compiling with CMake

```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . --parallel $(nproc)
ctest --output-on-failure
sudo cmake --install .
```

---

## 3. Building on macOS (Apple Silicon & Intel)

### 3.1 Prerequisites
Install Xcode Command Line Tools:
```bash
xcode-select --install
```

### 3.2 Compiling with Clang & Make

```bash
git clone https://github.com/nuratix/nextviper.git
cd nextviper

# Build
make -j$(sysctl -n hw.ncpu)

# Run tests
make test

# Install
cp bin/nextviper /usr/local/bin/
```

---

## 4. Building on Windows

### 4.1 Using Visual Studio & CMake

1. Open **Developer Command Prompt for VS 2022**.
2. Clone repository and run CMake:
   ```cmd
   git clone https://github.com/nuratix/nextviper.git
   cd nextviper
   mkdir build && cd build
   cmake .. -G "Visual Studio 17 2022" -A x64
   cmake --build . --config Release
   ```
3. The executable will be generated at `build\bin\Release\nextviper.exe`.

### 4.2 Using MSYS2 / MinGW-w64

```bash
pacman -S mingw-w64-x86_64-gcc mingw-w64-x86_64-cmake make
cd /c/path/to/nextviper
make -j4
./bin/nextviper.exe --version
```

---

## 5. Docker Containerized Build

A minimal, multi-stage Docker build:

```dockerfile
FROM ubuntu:22.04 AS builder
RUN apt-get update && apt-get install -y build-essential cmake git
WORKDIR /build
COPY . .
RUN make clean && make -j$(nproc) && make test

FROM ubuntu:22.04
COPY --from=builder /build/bin/nextviper /usr/local/bin/nextviper
ENTRYPOINT ["nextviper"]
```

---

## 6. Verifying Installation

Verify that NextViper is properly installed and active:

```bash
nextviper --version
# Output: NextViper 1.0.0 (Apex)

nextviper eval "print('NextViper is operational!')"
```
