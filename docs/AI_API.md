# NextViper AI Subsystem API Reference

The `ai` module provides a comprehensive suite of tools for deep learning and machine learning in NextViper.

```nextviper
import ai
import tensor
import data
```

---

## 1. Models (`ai.Sequential`, `ai.Model`)

### `ai.Sequential(layers: List[Module]) -> Sequential`
Constructs a sequential feedforward neural network from a list of layers.

#### Methods:
- **`model.compile(optimizer, loss, [metrics])`**: Configures training hyperparameters, loss function, and evaluation metrics.
- **`model.fit(x: Tensor, y: Tensor, epochs: int, batch_size: int, [verbose: bool]) -> History`**:
  Trains the model for a fixed number of epochs on input data `x` and target labels `y`. Returns a `History` object with loss trajectory.
- **`model.predict(x: Tensor) -> Tensor`**:
  Runs forward inference in evaluation mode. Returns predicted output tensor.
- **`model.evaluate(x: Tensor, y: Tensor) -> Map[String, Float]`**:
  Computes loss and evaluation metrics over the test dataset.
- **`model.summary()`**:
  Prints layer-by-layer architectural summary and trainable parameter counts.
- **`model.save(path: String)`**:
  Serializes model weights and architecture to safe `.nvmodel` binary/manifest format.
- **`model.train()`**:
  Sets network mode to training (enables dropout).
- **`model.eval()`**:
  Sets network mode to evaluation (disables dropout).
- **`model.zero_grad()`**:
  Resets all parameter gradients to zero.

---

## 2. Layers (`ai.Dense`, `ai.Dropout`, `ai.Flatten`)

### `ai.Dense(in_features: int, out_features: int, [activation: String], [bias: bool = true])`
Fully connected linear layer: $y = x W^T + b$.
- `activation`: Optional activation string: `"relu"`, `"sigmoid"`, `"tanh"`, `"softmax"`, or `"none"`.

### `ai.Dropout(p: float = 0.5)`
Randomly zeroes out input elements with probability $p$ during training.

### `ai.Flatten()`
Reshapes tensors of shape `[N, D1, D2, ...]` to `[N, D1 * D2 * ...]`.

### Activation Layers
- `ai.ReLU()`: Rectified linear unit $f(x) = \max(0, x)$.
- `ai.Sigmoid()`: Logistic sigmoid $f(x) = \frac{1}{1 + e^{-x}}$.
- `ai.Tanh()`: Hyperbolic tangent $f(x) = \tanh(x)$.
- `ai.Softmax([dim: int = -1])`: Normalized exponential probabilities.

---

## 3. Loss Functions (`ai.losses`)

- **`ai.MSE()` / `ai.MSELoss()`**: Mean Squared Error for regression.
- **`ai.MAE()` / `ai.MAELoss()`**: Mean Absolute Error for robust regression.
- **`ai.BCE()` / `ai.BCELoss()`**: Binary Cross-Entropy for 2-class classification.
- **`ai.CrossEntropy()` / `ai.CrossEntropyLoss()`**: Multi-class softmax cross-entropy.

---

## 4. Optimizers (`ai.optimizers`)

- **`ai.SGD(lr: float = 0.01, [momentum: float = 0.0], [weight_decay: float = 0.0])`**
  Stochastic gradient descent with momentum and L2 regularization.
- **`ai.Momentum(lr: float = 0.01, [momentum: float = 0.9])`**
  Classical momentum optimizer.
- **`ai.Adam(lr: float = 0.001, [beta1: float = 0.9], [beta2: float = 0.999], [eps: float = 1e-8], [weight_decay: float = 0.0])`**
  Adaptive moment estimation.
- **`ai.AdamW(lr: float = 0.001, [beta1: float = 0.9], [beta2: float = 0.999], [eps: float = 1e-8], [weight_decay: float = 0.01])`**
  Adam with decoupled weight decay.

---

## 5. Metrics (`ai.metrics`)

- **`ai.accuracy(y_pred: Tensor, y_true: Tensor) -> float`**: Percentage of correct classifications.
- **`ai.precision(y_pred: Tensor, y_true: Tensor) -> float`**: True Positives / (True Positives + False Positives).
- **`ai.recall(y_pred: Tensor, y_true: Tensor) -> float`**: True Positives / (True Positives + False Negatives).
- **`ai.f1_score(y_pred: Tensor, y_true: Tensor) -> float`**: Harmonic mean of precision and recall.
- **`ai.mae(y_pred: Tensor, y_true: Tensor) -> float`**: Mean absolute error.
- **`ai.mse(y_pred: Tensor, y_true: Tensor) -> float`**: Mean squared error.

---

## 6. Model Serialization (`ai.save`, `ai.load`)

- **`ai.save(model: Sequential, path: String)`**:
  Saves the model to `.nvmodel` format.
- **`ai.load(path: String) -> Sequential`**:
  Restores model architecture and parameters from `.nvmodel` format.

---

## 7. Autograd API (`tensor.autograd`)

- **`tensor.requires_grad: bool`**: Checks if tensor tracks gradients.
- **`tensor.set_requires_grad(requires_grad: bool)`**: Enables or disables gradient tracking.
- **`tensor.grad: Tensor`**: Accesses accumulated gradient tensor.
- **`tensor.backward([grad_output: Tensor])`**: Computes reverse-mode derivatives.
- **`tensor.zero_grad()`**: Resets gradient to null.
- **`tensor.detach() -> Tensor`**: Returns a detached copy that does not participate in autograd graph.
