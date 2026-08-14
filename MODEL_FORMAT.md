# NextViper Model Format (`.nvmodel`) Specification

## 1. Design Principles & Security Guarantee

Traditional machine learning serialization formats (such as Python `pickle`) are vulnerable to arbitrary code execution attacks. NextViper introduces the **`.nvmodel`** format designed around strict security and deterministic portability:

1. **Zero Code Execution**: The format only contains metadata, architectural layer declarations, tensor dimensions, and numerical weight matrices. No executable bytecode or arbitrary objects are evaluated.
2. **Human-Readable Header & Machine-Efficient Payloads**: Clear key-value sections for inspecting architecture, layer counts, and data types without specialized tools.
3. **Cross-Platform & Version-Safe**: Includes format version (`1.0`), model type, and target dtype.

---

## 2. File Format Structure

An `.nvmodel` file consists of structured text sections:

```
[nextviper_model]
version = 1.0
type = Sequential
num_layers = 2
dtype = FLOAT32

[layer.0]
type = Dense
in_features = 2
out_features = 8
activation = relu
bias = true
weights = 0.123,-0.456,0.789,...
biases = 0.01,-0.02,...

[layer.1]
type = Dense
in_features = 8
out_features = 1
activation = sigmoid
bias = true
weights = 0.987,-0.654,...
biases = 0.05,...
```

---

## 3. Supported Layer Encodings

| Layer Type | Serialized Parameters | Notes |
|:---|:---|:---|
| `Dense` | `in_features`, `out_features`, `activation`, `bias`, `weights`, `biases` | Weights flattened row-major comma-separated floats |
| `Dropout` | `p` | Inverted dropout rate (disabled during eval) |
| `Flatten` | (no trainable parameters) | Shape reshape node |
| `ReLU` / `Sigmoid` / `Tanh` / `Softmax` | (optional `dim`) | Activation operators |

---

## 4. Reading and Writing `.nvmodel`

### Saving a Model
```nextviper
import ai

let model = ai.Sequential([...])
model.save("my_network.nvmodel")
```

### Loading a Model
```nextviper
import ai

let model = ai.load("my_network.nvmodel")
let preds = model.predict(input_tensor)
```

In C++, serialization is handled by `nextviper::ModelSerializer::save` and `nextviper::ModelSerializer::load`.
