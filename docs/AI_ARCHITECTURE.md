# NextViper AI/ML Subsystem Architecture

## 1. Overview and Core Philosophy

NextViper is a modern, high-performance, general-purpose programming language. The AI/ML subsystem in NextViper is architected entirely as a modular library and runtime system built cleanly on top of:
1. **NextViper Core**: Lexer, parser, type checker, and bytecode runtime / native engine.
2. **Standard Library**: High-performance I/O, file system, math, and serialization.
3. **Data Subsystem**: `data.DataFrame`, `data.DataArray`, `data.Dataset`, schemas, cleaning, and preprocessing.
4. **Tensor Subsystem**: Multidimensional n-dimensional arrays, device abstractions, linear algebra, and broadcasting.

**Strict Non-Goal**: NextViper does *not* introduce AI-specific keywords or syntax into the core language grammar. Machine learning workflows are expressed with clean, idiomatic NextViper APIs.

```
+-------------------------------------------------------------------------+
|                      NextViper Application Code                         |
+-------------------------------------------------------------------------+
|      Data Loading & Preprocessing      |     Model Definition & Eval    |
|      (data.DataFrame, data.Dataset)    |     (ai.Sequential, ai.Dense)  |
+----------------------------------------+--------------------------------+
|                         NextViper AI Subsystem                          |
|  +-------------------------------------------------------------------+  |
|  | Models | Layers | Loss Functions | Optimizers | Metrics | Training |  |
|  +-------------------------------------------------------------------+  |
|  |                    Automatic Differentiation (Autograd)           |  |
|  |           Dynamic Computational Graph & Backward Nodes            |  |
|  +-------------------------------------------------------------------+  |
|  |                         Tensor Subsystem                          |  |
|  |            N-D Arrays, Broadcasting, Strided Memory               |  |
|  +-------------------------------------------------------------------+  |
|  |                      Compute Backend Abstraction                  |  |
|  |                CPU (AVX/Threads)  |  GPU Backend (Future)         |  |
+--+-------------------------------------------------------------------+--+
|                    NextViper Bytecode VM & Runtime Engine               |
+-------------------------------------------------------------------------+
```

---

## 2. Dynamic Autograd & Computational Graph

NextViper uses a reverse-mode automatic differentiation (autograd) engine similar to PyTorch.

### Computation Graph Representation
- **Tensors as Nodes**: Every `Tensor` holds an `AutogradMeta` container (shared across copies) specifying `requires_grad`, `is_leaf`, accumulated `grad`, and a pointer to creator `grad_fn`.
- **Backward Operations (`AutogradNode`)**: Operation backward nodes (`AddBackwardNode`, `MulBackwardNode`, `MatMulBackwardNode`, `ReLUBackwardNode`, etc.) save tensors needed for the backward pass and define the analytical derivative computation `backward(grad_output)`.
- **Topological Traversal & DAG Gradient Routing**: When `tensor.backward()` is called, the graph is traversed in reverse topological order. Intermediate gradients are summed across branches and accumulated into leaf parameter `.grad` tensors.

### Key Invariants
- **No-Grad Guarding**: Inference, metric calculation, and gradient checking run within `AutogradContext::NoGradGuard` to disable graph construction and eliminate memory overhead.
- **Gradient Accumulation**: Gradients accumulate additively (`inp.grad = inp.grad + input_grad`), enabling gradient accumulation across multiple micro-batches before `optimizer.step()`.
- **Zeroing Gradients**: `optimizer.zero_grad()` or `model.zero_grad()` clears accumulated gradients prior to each forward-backward cycle.

---

## 3. Layer Architecture (`ai.layers`)

Layers derive from the base `Module` / `Layer` interface:
```cpp
class Module {
public:
    virtual ~Module() = default;
    virtual Tensor forward(const Tensor& input) = 0;
    virtual std::vector<std::shared_ptr<Parameter>> parameters() = 0;
    virtual std::vector<std::shared_ptr<Parameter>> trainable_parameters();
    virtual void train(bool mode = true);
    virtual void eval();
};
```

### Supported Core Layers:
1. **`Dense(in_features, out_features, [activation], [bias])`**: Fully connected linear layer with Xavier uniform weight initialization and optional bias.
2. **`Dropout(p)`**: Inverted dropout with probability $p$. Scales activations during training by $1 / (1 - p)$ and acts as identity during `eval()`.
3. **`Flatten()`**: Flattens multi-dimensional tensors to 2D matrices `[batch_size, -1]`.
4. **Non-Linear Activations**: `ReLU`, `Sigmoid`, `Tanh`, `Softmax`.

### Extensibility Roadmap:
The module abstraction allows straightforward additions of `Conv2D`, `BatchNorm`, `LayerNorm`, `Embedding`, `MultiHeadAttention`, and `TransformerBlock` layers.

---

## 4. Loss Functions & Optimizers

### Loss Functions (`ai.losses`)
- **`MSELoss`**: Mean Squared Error $\mathcal{L} = \frac{1}{N} \sum (y - \hat{y})^2$.
- **`MAELoss`**: Mean Absolute Error $\mathcal{L} = \frac{1}{N} \sum |y - \hat{y}|$.
- **`BCELoss`**: Binary Cross-Entropy with numerical stabilization $\epsilon = 10^{-12}$.
- **`CrossEntropyLoss`**: Multi-class softmax cross-entropy.

### Optimizers (`ai.optimizers`)
- **`SGD(lr, momentum, weight_decay)`**: Stochastic Gradient Descent with classical momentum.
- **`Adam(lr, beta1, beta2, eps, weight_decay)`**: Adaptive Moment Estimation with bias-corrected first and second moment estimates.
- **`AdamW(lr, beta1, beta2, eps, weight_decay)`**: Decoupled weight decay regularization.

---

## 5. Model Training & Lifecycle

NextViper models implement a standard lifecycle:
1. **Compilation**: `model.compile(optimizer, loss, [metrics])` sets optimizer parameter references and evaluation targets.
2. **Fitting**: `model.fit(x, y, epochs, batch_size, [val_x], [val_y])` executes mini-batch training, shuffling, forward passes, loss computation, backpropagation, optimizer steps, and metric logging.
3. **History**: Returns a `History` object containing loss curves and metrics per epoch.
4. **Inference**: `model.eval()` disables dropout, and `model.predict(x)` produces deterministic predictions.
5. **Serialization**: `model.save(path)` and `ai.load(path)` safely persist and restore model architecture and weights without code execution vulnerabilities.

---

## 6. Hardware Backend Abstraction

The compute backend is abstracted behind `TensorBackend`:
```
+---------------------------------------------+
|               Tensor Operations             |
+---------------------------------------------+
                       |
       +---------------+---------------+
       |                               |
+---------------+              +---------------+
|  CPU Backend  |              |  GPU Backend  |
|  (Vectorized  |              |  (CUDA/Vulkan |
|   AVX/Threads)|              |   Future)     |
+---------------+              +---------------+
```
All tensor allocations, copies, and kernel dispatches go through the backend interface, enabling drop-in GPU backend implementations without modifying the model or autograd layers.
