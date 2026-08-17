# NextViper Native Dependencies & Prerequisites

## Core Build & Runtime Dependencies

### 1. Build Tools & Compilers
- **C++ Compiler**: Modern C++20 compliant compiler (`g++ >= 11` or `clang++ >= 14`).
- **C Compiler (for Native AOT Compilation)**: `gcc` or `clang`.
- **GNU Make**: Build automation (`make >= 4.0`).

### 2. Native System Libraries
- **Vulkan Driver & Headers (`libvulkan-dev`, `libvulkan1`)**: Required for GPU compute acceleration, shader dispatch, and Vulkan tensor backends.
- **PostgreSQL Client Library (`libpq-dev`, `libpq5`)**: Required for `std.db` PostgreSQL connection, parameterized query execution, and transactions.
- **POSIX Threads (`pthread`)**: Required for multi-threaded runtime, HTTP request loop, and `std.concurrency`.
- **Standard C Math Library (`libm`)**: Required for floating-point math, trigonometry, and autograd kernels.

### Installation Instructions by Platform

#### Debian / Ubuntu / Linux Mint
```bash
sudo apt-get update
sudo apt-get install -y build-essential libvulkan-dev vulkan-tools libpq-dev
```

#### Fedora / RHEL / Rocky Linux
```bash
sudo dnf install -y gcc-c++ make vulkan-headers vulkan-loader-devel libpq-devel
```

#### Arch Linux / Manjaro
```bash
sudo pacman -S base-devel vulkan-devel postgresql-libs
```

#### macOS (Homebrew)
```bash
brew install molten-vk libpq
```
