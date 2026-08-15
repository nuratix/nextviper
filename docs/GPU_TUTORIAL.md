# NextViper GPU Acceleration Tutorial

This tutorial walks through writing high-performance numerical and machine learning pipelines accelerated by GPU compute in NextViper.

---

## 1. Quickstart: Matrix Multiplication on GPU

Create `gpu_gemm.nv`:

```nextviper
import tensor
import time

let size = 1024

print("Matrix Multiplication Benchmark (" + str(size) + "x" + str(size) + ")")

// CPU Matrix Multiplication
let t0 = time.now()
let a_cpu = tensor.randn([size, size])
let b_cpu = tensor.randn([size, size])
let c_cpu = a_cpu.matmul(b_cpu)
let cpu_ms = time.elapsed_ms(t0)
print("CPU Matmul Time:", cpu_ms, "ms")

// GPU Matrix Multiplication
if tensor.is_gpu_available() {
    let t1 = time.now()
    let a_gpu = a_cpu.to("gpu")
    let b_gpu = b_cpu.to("gpu")
    let c_gpu = a_gpu.matmul(b_gpu)
    let gpu_ms = time.elapsed_ms(t1)
    print("GPU Matmul Time:", gpu_ms, "ms")
    print("Speedup:", round(cpu_ms / gpu_ms, 2), "x")
} else {
    print("GPU acceleration not available on this device.")
}
```

Run:
```bash
nextviper run gpu_gemm.nv
```

---

## 2. Neural Network Training on GPU

Create `gpu_training.nv`:

```nextviper
import tensor
import ai

print("Training Neural Network on GPU...")

// Generate synthetic dataset
let num_samples = 2000
let input_dim = 64
let num_classes = 10

let x_train = tensor.randn([num_samples, input_dim], 0.0, 1.0, "gpu")
let y_train = tensor.zeros([num_samples, num_classes], "gpu")

// Build Model
let model = ai.Sequential([
    ai.Dense(128, "relu"),
    ai.Dense(64, "relu"),
    ai.Dense(num_classes, "softmax")
])

// Move Model to GPU
model = model.to("gpu")
print("Model running on:", model.device())

// Compile with Optimizer and Loss
model.compile(
    ai.Adam(model.trainable_parameters(), lr: 0.001),
    ai.CrossEntropyLoss(),
    metrics: ["accuracy"]
)

// Train model on GPU
let history = model.fit(x_train, y_train, epochs: 10, batch_size: 64)
print("Training completed successfully on GPU!")

// Run Inference
let test_x = tensor.randn([5, input_dim], 0.0, 1.0, "gpu")
let preds = model.predict(test_x)
print("Predictions shape:", preds.shape)
```

Run:
```bash
nextviper run gpu_training.nv
```
