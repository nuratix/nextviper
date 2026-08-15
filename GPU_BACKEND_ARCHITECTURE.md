# NextViper GPU Backend Architecture & Evaluation

This document outlines the architectural evaluation and technical decision for NextViper's production hardware accelerator and GPU subsystem.

---

## 1. Executive Summary & Decision

NextViper has selected **Vulkan Compute / SPIR-V** as its primary hardware acceleration backend.

Vulkan Compute provides an open, modern, vendor-neutral compute pipeline capable of executing high-performance parallel shaders on NVIDIA, AMD, Intel, Apple Silicon (via MoltenVK), ARM Mali/Adreno mobile SoCs, and virtualized/headless cloud environments (via Mesa Lavapipe).

```
+-----------------------------------------------------------------------+
|                         NextViper User Code                           |
|       let x = tensor.random([1024, 1024], device: "gpu")              |
|       let model = ai.Sequential([...]).to("gpu")                      |
+-----------------------------------------------------------------------+
                                  |
                                  v
+-----------------------------------------------------------------------+
|                    NextViper Tensor & AI Subsystem                    |
|             Unified Tensor API & Autograd Dynamic Graph Engine        |
+-----------------------------------------------------------------------+
                                  |
                                  v
+-----------------------------------------------------------------------+
|                 Device Abstraction Layer (TensorBackend)               |
|            +-----------------------+   +------------------------+     |
|            |    CPUTensorBackend   |   |    GPUTensorBackend    |     |
|            +-----------------------+   +------------------------+     |
+-----------------------------------------------------------------------+
                                                     |
                                                     v
+-----------------------------------------------------------------------+
|                     Vulkan Compute Implementation                     |
|  - Host/Device Persistent Memory & Staging Buffers                     |
|  - Precompiled Embedded SPIR-V Bytecode Compute Shaders               |
|  - 2D Workgroup Tiled GEMM Pipeline                                   |
|  - Elementwise, Reduction & In-Device Optimizer Kernels               |
|  - VkInstance / VkDevice / VkCommandPool / VkQueue Engine             |
+-----------------------------------------------------------------------+
                                  |
                                  v
+-----------------------------------------------------------------------+
|                        Physical GPU Drivers                           |
|   NVIDIA (CUDA Cores) | AMD (ROCm/RDNA) | Intel (Arc/Iris) | Apple    |
+-----------------------------------------------------------------------+
```

---

## 2. Comprehensive Evaluation of GPU Backends

| Backend | Vendor Neutrality | Cross-Platform | Setup Complexity | Headless / CI Support | Embedded Portability | Decision |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| **Vulkan Compute** | **Universal (Khronos)** | **Linux, macOS, Windows, Android** | **Standard Vulkan Driver** | **Native (Lavapipe)** | **High (Embedded SPIR-V)** | **SELECTED (Primary Backend)** |
| **CUDA** | Proprietary (NVIDIA only) | Linux, Windows | Requires CUDA Toolkit (Gigabytes) | Requires NVIDIA Hardware | Low | Future Plugin Candidate |
| **ROCm / HIP** | Proprietary (AMD only) | Linux | Heavy ROCm stack | Requires AMD Hardware | Low | Future Plugin Candidate |
| **Metal** | Proprietary (Apple only) | macOS, iOS | Apple Developer Toolchain | macOS Only | Low | Future Backend Candidate |
| **WebGPU / Dawn**| Browser / Native Hybrid | Linux, macOS, Windows | Large C++ runtime dependency | Medium | Medium | Evaluated for Web / WASM |
| **OpenCL** | Universal (Khronos) | Linux, macOS, Windows | Inconsistent modern driver support | Low | Medium | Legacy |

### Detailed Evaluation Rationale

1. **Vendor Neutrality**:
   CUDA is restricted exclusively to NVIDIA GPUs. Adopting CUDA as the sole backend would exclude AMD Radeon, Apple Silicon, Intel Arc, and ARM devices. Vulkan Compute runs across all modern GPU vendors with unified SPIR-V binaries.

2. **Compilation Speed & Self-Contained Deployment**:
   NextViper embeds precompiled SPIR-V binary kernels directly into the runtime binary (`gpu_shaders_spirv.hpp`). No runtime compiler toolchain (`nvcc`, `hipcc`, or runtime GLSL parser) is required on end-user machines. The binary interacts directly with the system's Vulkan driver (`libvulkan.so.1`).

3. **Performance & Predictability**:
   Vulkan compute shaders provide explicit workgroup sizing, shared memory caching (`tileA`, `tileB`), barrier synchronizations, push constant uniforms, and zero-overhead Vulkan descriptor pipelines.

4. **Future Extensibility**:
   The `GPUTensorBackend` class provides an extensible abstract interface. Future dedicated backends (such as direct CUDA or Metal) can be added as specialized modular drivers without altering NextViper language syntax or tensor APIs.

---

## 3. GPU Memory Management Architecture

NextViper uses unified `GPUBuffer` allocations:
- **Zero-Copy Host Visible + Coherent Allocations**: When tensors require host inspection (`tensor.get()`, `tensor.to_string()`), memory is persistently mapped without expensive buffer recreation.
- **In-Device Resident Operations**: When chaining operations (e.g., `let z = x.matmul(y).relu().add(bias)`), intermediate tensors remain 100% on GPU device memory. No CPU memory transfers occur until explicitly requested via `.to("cpu")` or `.to_list()`.
- **Automatic RAII Resource Deallocation**: GPU buffers and device memory allocations are bound to C++ `std::shared_ptr` custom deleters, automatically invoking `vkFreeMemory` and `vkDestroyBuffer` upon garbage collection.

---

## 4. Hardware Fallback Policy

- `device: "gpu"`: Strict mode. If no compatible physical GPU is detected, NextViper immediately throws a clear `RuntimeError` describing the missing device, rather than silently pretending GPU compute occurred.
- `device: "auto"`: Adaptive mode. NextViper checks `GPUTensorBackend::is_gpu_available()`. If a GPU is detected, execution runs on GPU; otherwise, it executes on CPU.
