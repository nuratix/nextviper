# NextViper GPU Acceleration Subsystem Architecture

This document provides a technical overview of NextViper's GPU runtime, compute pipeline execution model, autograd DAG routing, and in-device memory resident optimization.

---

## 1. Architectural Principles

1. **Syntactic Purity**: Existing NextViper user code requires zero GPU-specific keywords or syntax alterations. The same codebase runs on CPU or GPU by changing device configuration (`tensor.to("gpu")` or `device: "gpu"`).
2. **GPU Data Residency**: Chained operations stay resident in GPU VRAM across consecutive kernels. intermediate results are not copied back to CPU memory between matrix multiplications, activations, or losses.
3. **Hardware Transparency**: Tensors report their current physical device via `tensor.device()`. Device-to-device transfers are explicit and zero-overhead.
4. **Deterministic Autograd on GPU**: Reverse-mode automatic differentiation DAG backward nodes execute on GPU buffers without CPU roundtrips.

---

## 2. Component Pipeline

```
+-------------------------------------------------------------+
|                     NextViper Frontend                      |
|                  AST -> Interpreter / VM                    |
+-------------------------------------------------------------+
                              |
                              v
+-------------------------------------------------------------+
|                  Unified Tensor Abstraction                 |
|               Shape | DType | Device | Strides              |
+-------------------------------------------------------------+
                              |
              +---------------+---------------+
              |                               |
              v                               v
+-----------------------------+ +-----------------------------+
|       CPUTensorBackend      | |       GPUTensorBackend      |
|  - SIMD / OpenMP vectorization |  - Vulkan Compute SPIR-V    |
|  - Cache-blocked matrix ops |  - 2D Workgroup Tiled GEMM   |
|  - Host memory allocation   |  - Persistent GPU Buffer pool |
+-----------------------------+ +-----------------------------+
```

---

## 3. GPU Compute Shaders & Kernel Execution

NextViper ships with precompiled SPIR-V binaries for all core deep learning and tensor kernels:

### 1. Elementwise Binary & Unary Shaders (`eltwise_binary.comp`, `eltwise_unary.comp`)
- Compute $C[i] = A[i] \odot B[i]$ for addition, subtraction, elementwise multiplication, and division.
- Unary activations: ReLU ($max(0, x)$), Sigmoid ($\frac{1}{1 + e^{-x}}$), Tanh, Exp, Log, Negation, Absolute value.
- Configured with 256-thread 1D local workgroups.

### 2. High-Performance GEMM Shader (`matmul.comp`)
- 2D workgroup tiled matrix multiplication using $16 \times 16$ thread workgroups.
- Uses shared local memory (`shared float tileA[16][16]`, `shared float tileB[16][16]`) to maximize cache reuse and memory bandwidth across GPU shader cores.
- Handles arbitrary matrix shapes $M \times K \times N$ with boundary guards.

### 3. Parallel Reduction Shader (`reduce.comp`)
- Parallel tree reduction for `sum`, `min`, `max`, and `mean`.
- Workgroup tree reduction in local memory with `barrier()` synchronization.

### 4. GPU Optimizer Shader (`optimizer.comp`)
- Parameter updates run 100% in-device without host roundtrips.
- Implements:
  - **SGD with Weight Decay**: $p \leftarrow p - \eta (g + \lambda p)$
  - **Momentum**: $v \leftarrow \beta v + g; p \leftarrow p - \eta v$
  - **Adam / AdamW**: First and second moment running averages maintained entirely in GPU VRAM buffers.

---

## 4. Host <-> Device Memory Transfers

```
[ Host CPU RAM ]                     [ GPU Device Memory ]
   t_cpu.data_       ---- .to("gpu") ---->   VkBuffer / VkDeviceMemory
                     <--- .to("cpu") <----
```

- When moving to GPU, NextViper maps data to host-visible, coherent GPU buffers.
- GPU tensors maintain their allocated memory until the `Tensor` object is destroyed, at which point the custom `std::shared_ptr` deleter destroys the Vulkan buffer.
