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
sudo apt-get install -y build-essential libvulkan-dev vulkan-tools libpq-dev libcurl4-openssl-dev
```

#### Fedora / RHEL / Rocky Linux
```bash
sudo dnf install -y gcc-c++ make vulkan-headers vulkan-loader-devel libpq-devel libcurl-devel
```

#### Arch Linux / Manjaro
```bash
sudo pacman -S base-devel vulkan-devel postgresql-libs
```

#### macOS (Homebrew)
```bash
brew install molten-vk libpq
```

Incremental HTTP (`http.exchange` / `http.stream`) additionally links libcurl 7.85 or later. Debian/Ubuntu development package: `libcurl4-openssl-dev`. See [transport APIs](TRANSPORT_API.md). Older distribution libcurl versions need an upgrade before building these additions.

The HTTP transport integration tests use Python 3 standard-library fixtures and the OpenSSL command-line program to generate a temporary test certificate. These are test dependencies; the `.nv` AI SDK does not depend on Python.
