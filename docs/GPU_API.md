# NextViper GPU Acceleration API Reference

NextViper provides high-performance, vendor-neutral GPU computing built directly into the `tensor` and `ai` standard modules.

---

## 1. Device Inspection & Configuration

### `tensor.is_gpu_available() -> bool`
Returns `true` if a compatible GPU (NVIDIA, AMD, Intel, Apple Silicon, or Vulkan compute device) is detected and initialized; otherwise `false`.

```nextviper
import tensor

if tensor.is_gpu_available() {
    print("GPU acceleration is active!")
}
```

### `tensor.device_count() -> int`
Returns the number of available compute devices.

```nextviper
let count = tensor.device_count()
print("Compute devices detected:", count)
```

### `tensor.device_name([index: int]) -> string`
Returns the physical device name of the GPU (e.g. `"NVIDIA GeForce RTX 4090"`, `"AMD Radeon RX 7900 XTX"`, or `"Apple M3 Max"`).

```nextviper
print("Active accelerator:", tensor.device_name())
```

### `tensor.default_device() -> string`
Returns the current default device (`"cpu"`, `"gpu"`, or `"auto"`).

```nextviper
print("Default execution target:", tensor.default_device())
```

### `tensor.set_default_device(device: string)`
Sets the default target device for all subsequent tensor and model instantiations.
- Throws a descriptive `RuntimeError` if `"gpu"` is requested when no GPU is present.

```nextviper
tensor.set_default_device("gpu")
```

---

## 2. Tensor Device Methods

### `tensor.device() -> string`
Returns `"gpu"` or `"cpu"` representing where the tensor data currently resides.

```nextviper
let x = tensor.create([1.0, 2.0, 3.0])
print(x.device()) // "cpu"
```

### `tensor.to(device: string) -> Tensor`
Transfers the tensor to the target device (`"gpu"`, `"cpu"`, or `"auto"`).
- `t.to("gpu")`: Copies data from Host RAM to GPU VRAM.
- `t.to("cpu")`: Copies data from GPU VRAM back to Host RAM.
- `t.to("auto")`: Selects `"gpu"` if available, otherwise `"cpu"`.
- If the tensor is already on the target device, returns the tensor directly without reallocating.

```nextviper
let t_cpu = tensor.randn([512, 512])
let t_gpu = t_cpu.to("gpu")
print(t_gpu.device()) // "gpu"
```

---

## 3. Direct GPU Tensor Creation

All tensor factory functions accept an optional target device argument:

```nextviper
import tensor

// Direct GPU allocation without initial CPU copy
let x = tensor.zeros([1024, 1024], "gpu")
let y = tensor.ones([1024, 1024], "gpu")
let w = tensor.randn([1024, 1024], 0.0, 1.0, "gpu")
let u = tensor.uniform([1024, 1024], -1.0, 1.0, "gpu")
```

---

## 4. GPU-Accelerated Mathematical Operations

When tensors reside on GPU, all arithmetic, matrix multiplications, activations, and reductions run 100% on GPU shader cores:

```nextviper
import tensor

let a = tensor.randn([2048, 2048], "gpu")
let b = tensor.randn([2048, 2048], "gpu")

// GPU GEMM (Matrix Multiplication)
let c = a.matmul(b)

// GPU Elementwise Operations
let d = c.add(a).sub(b).mul(c).div(a)

// GPU Scalar Operations
let s = d.scalar_add(10.0).scalar_mul(0.5)

// GPU Activations
let r = s.relu()
let sig = s.sigmoid()
let t = s.tanh()

// GPU Reductions
let sum_val = r.sum()
let max_val = r.max()
let min_val = r.min()

// GPU Transpose
let transposed = a.T()
```

---

## 5. AI Models on GPU

### Moving Models to GPU

```nextviper
import ai

let model = ai.Sequential([
    ai.Dense(512, "relu"),
    ai.Dropout(0.2),
    ai.Dense(256, "relu"),
    ai.Dense(10, "softmax")
])

// Transfer all layer weights and biases to GPU VRAM
model = model.to("gpu")
print("Model device:", model.device()) // "gpu"
```

### In-Device Forward Pass & Training

```nextviper
model.compile(
    ai.Adam(model.trainable_parameters(), lr: 0.001),
    ai.CrossEntropyLoss(),
    metrics: ["accuracy"]
)

// Forward pass and training loop execute entirely on GPU
let x_gpu = tensor.randn([1000, 512], "gpu")
let y_gpu = tensor.zeros([1000, 10], "gpu")

let history = model.fit(x_gpu, y_gpu, epochs: 20, batch_size: 64)
let predictions = model.predict(x_gpu)
```

---

## 6. Error Handling

When GPU acceleration is unavailable, explicit error messages are produced without silent fallback:

```nextviper
// If no GPU is available:
// RuntimeError: GPU unavailable: No compatible GPU or Vulkan compute device found on this system.
let t = tensor.zeros([10, 10], "gpu")
```

For adaptive applications, use `"auto"`:
```nextviper
let t = tensor.zeros([10, 10], "auto") // Selects GPU if present, otherwise CPU
```
