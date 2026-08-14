# NextViper AI & Data Architecture Specification

**Version:** 0.1.0  
**Status:** Implemented & Verified  
**Scope:** Core Abstractions, Tensors, Datasets, Neural Networks, Inference & Extensible Hardware Acceleration

---

## 1. Architectural Philosophy

NextViper is a **modern general-purpose programming language**. It does not bind the language grammar itself to specific AI frameworks or vendors. Instead, high-performance data processing and AI capabilities are provided as first-class, standard library modules (`data`, `tensor`, `ai`, `nn`) built atop a unified native engine with zero artificial runtime overhead.

```
+-------------------------------------------------------------------------+
|                       NextViper Application Code                        |
|   import data                  import tensor               import ai    |
+-------------------------------------------------------------------------+
|                  NextViper Standard Ecosystem APIs                      |
|   Dataset & DataLoader     |   N-D Tensor Engine   |   AIModel & Layers |
+-------------------------------------------------------------------------+
|                  Pluggable Hardware & Runtime Backends                  |
|    CPUTensorBackend   |   CUDATensorBackend (GPU)  |   ONNX / Runtime   |
+-------------------------------------------------------------------------+
```

---

## 2. Abstractions & Core Interfaces

### 2.1 Multi-Dimensional Tensor (`Tensor`)
The `Tensor` class is an $N$-dimensional array supporting arbitrary shapes, strides, and memory layouts with contiguous/non-contiguous views.

- **Data Types (`DType`)**: `FLOAT32`, `FLOAT64`, `INT32`, `INT64`
- **Device Support (`Device`)**: `CPU`, `CUDA`, `MPS`, `CUSTOM`
- **Memory Layout**: Reference-counted buffer `std::shared_ptr<void>` with cache-aligned storage.

#### Key Operations:
- **Matrix Multiplication**: $C = A \times B$ (`matmul`, `T()`, shape broadcasting).
- **Elementwise Math**: `add`, `sub`, `mul`, `div`, `scalar_add`, `scalar_mul`.
- **Reductions**: `sum(dim)`, `mean(dim)`, `max(dim)`, `min(dim)`, `argmax(dim)`.
- **Nonlinear Activations**: `relu()`, `sigmoid()`, `tanh()`, `softmax(dim)`.

```nextviper
import tensor
import ai

let A = ai.tensor([2, 3], [1.0, 2.0, 3.0, 4.0, 5.0, 6.0])
let B = ai.tensor([3, 2], [7.0, 8.0, 9.0, 10.0, 11.0, 12.0])
let C = A.matmul(B) // Result: [2, 2] tensor
```

---

### 2.2 Tabular Datasets & Preprocessing (`Dataset`)
The `Dataset` abstraction provides structured tabular schema handling for real data ingestion, preprocessing, and feature engineering.

- **Data Ingestion**: High-speed CSV parsing (`data.load("path.csv")`, `data.from_csv(string)`), in-memory row/column construction (`data.from_rows(cols, rows)`).
- **Missing Value Handling**: `clean(drop_nulls=true)` or `clean(drop_nulls=false, fill_strategy="mean" | "zero")`.
- **Transformation**: `shuffle(seed)`, `split(train_ratio)`, `select([columns])`, `head(n)`.
- **Summary Statistics**: `describe()` (count, mean, std, min, max).
- **Tensor Conversion**: `to_tensor(columns)` produces a 2D numerical Tensor ready for training or inference.

```nextviper
import data

let df = data.load("dataset.csv")
let clean_df = df.clean(false, "mean")
let splits = clean_df.shuffle(42).split(0.8)

let train_data = splits[0]
let test_data = splits[1]

let X_train = train_data.select(["age", "income", "score"]).to_tensor()
```

---

### 2.3 Mini-Batching (`DataLoader`)
The `DataLoader` class abstracts data iteration and mini-batching during model training and evaluation:
- Batch slicing with configurable `batch_size`
- Deterministic or random shuffling
- `drop_last` option for handling incomplete tail batches

---

### 2.4 Neural Networks & Models (`AIModel`)
The `AIModel` abstraction encapsulates layered neural networks, inference pipelines, and gradient-based parameter updates.

- **Layer System**:
  - `LinearLayer(in_features, out_features, bias=true)`: Xavier/Glorot uniform initialization, forward pass ($Y = X W^T + b$), backward pass with gradient accumulation ($dW = dY^T X$, $db = \sum dY$, $dX = dY W$).
  - `ActivationLayer`: `ReLU`, `Sigmoid`, `Tanh`, `Softmax`.
- **Inference (`predict`)**:
  - Accepts raw tensors or datasets, computing forward pass outputs.
- **Training (`train_step` / `fit`)**:
  - Computes Mean Squared Error (MSE) loss or Cross-Entropy loss.
  - Backpropagates error gradients through all layers.
  - Updates weights with learning rate schedule.
- **Model Serialization (`save` / `load`)**:
  - Saves network architecture, dimensions, weights, and biases to `.nvmodel` format with full double-precision floating point fidelity.
  - Loads pretrained weights directly for zero-cost cold starts.

```nextviper
import ai

// Construct model
let model = ai.linear(3, 1)

// Predict on input tensor
let predictions = model.predict(X_train)

// Save model weights
model.save("models/linear_regressor.nvmodel")

// Reload model
let loaded_model = ai.load("models/linear_regressor.nvmodel")
let new_pred = loaded_model.predict(X_test)
```

---

## 3. Pluggable Hardware Acceleration Architecture

The hardware execution layer is decoupled via the `TensorBackend` abstract interface:

```cpp
class TensorBackend {
public:
    virtual ~TensorBackend() = default;
    virtual std::string name() const = 0;
    virtual Device device() const = 0;
    virtual bool is_available() const = 0;

    virtual std::shared_ptr<void> allocate(size_t bytes) = 0;
    virtual void copy(void* dst, const void* src, size_t bytes) = 0;
    virtual void fill(void* data, size_t count, double val, DType dtype) = 0;
};
```

- **`CPUTensorBackend`**: High-performance CPU compute with SIMD cache-line alignment.
- **Pluggable Accelerators**: Interfaces ready for `CUDATensorBackend` (NVIDIA GPUs), `MPSTensorBackend` (Apple Silicon), and external inference runtimes (ONNX Runtime, LibTorch, TensorRT).
