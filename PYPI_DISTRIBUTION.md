# PyPI / Python Distribution Evaluation

**Status**: PLANNED (Scoped to Python Language Bindings only)  
**Evaluation Target**: `pip install nextviper`

---

## 1. Technical Evaluation

NextViper was evaluated to determine whether `pip` / PyPI is an appropriate distribution channel for the NextViper compiler and toolchain.

### 1.1 Architecture Analysis
- **Core NextViper Toolchain**: A standalone native C++20 compiler, virtual machine, and Vulkan GPU compute engine compiled into native ELF / Mach-O / PE binaries (`bin/nextviper` and `bin/nextviper-lsp`).
- **Python Ecosystem Role**: PyPI is designed for Python libraries, CPython extensions, and wheel packages.

### 1.2 Evaluation Findings
1. **Packaging a Native Compiler as a Wheel**: While possible (e.g. `cmake` or `ninja` on PyPI), distributing a full programming language compiler exclusively through PyPI forces users to depend on Python and pip environments, polluting virtual environments and creating confusion around package ownership.
2. **Fake Wrapper Prohibition**: Creating a dummy Python package that merely runs `curl` to download a binary during `setup.py` violates packaging standards and creates security risks.

---

## 2. Decision & Scoping

- **Decision**: The NextViper compiler and CLI will **NOT** be distributed as a global CLI binary via PyPI.
- **Future Python Bindings (`nextviper-py`)**: An official Python extension module (`nextviper-py`) will be published to PyPI in a future milestone to provide zero-copy tensor interoperability between NextViper arrays and NumPy / PyTorch tensors.
