# NextViper Standard Library API Reference (v1.0.0)

NextViper provides a battery of carefully curated, high-performance standard libraries built for systems programming, data engineering, mathematical computation, and AI inference.

---

## 1. Global Built-in Functions

Global functions are always available in any NextViper source file without requiring imports.

### 1.1 I/O & Conversion
- `print(value: any) -> nil`: Outputs string representation of `value` to standard output followed by a newline.
- `str(value: any) -> string`: Converts any value (int, float, bool, list, map, tensor) to string.
- `int(value: any) -> int`: Parses integer value or truncates floating point.
- `float(value: any) -> float`: Parses floating point value from integer or string.
- `bool(value: any) -> bool`: Evaluates truthiness of a value.

### 1.2 Sequences & Ranges
- `len(collection: list | map | string) -> int`: Returns length of collection.
- `range(start: int, stop: int, step: int = 1) -> list[int]`: Generates a list of integers from `start` up to `stop` with resource-bounded safety limits (up to 10,000,000 items).

---

## 2. Collection Methods

### 2.1 List Methods (`list[T]`)
- `list.append(item: T) -> nil`: Appends element to end of list.
- `list.push(item: T) -> nil`: Alias for `append`.
- `list.insert(index: int, item: T) -> nil`: Inserts element at specified index.
- `list.remove(index: int) -> T`: Removes and returns element at specified index.
- `list.len() -> int`: Returns number of elements.
- `list.slice(start: int, end: int) -> list[T]`: Returns sublist `[start..end]`.
- `list.map(fn: (T) -> U) -> list[U]`: Applies transform function to every item.
- `list.filter(fn: (T) -> bool) -> list[T]`: Filters items matching predicate.
- `list.reduce(initial: U, fn: (acc: U, item: T) -> U) -> U`: Folds list into accumulated value.

### 2.2 Map Methods (`map[string, T]`)
- `map.has(key: string) -> bool`: Checks if key exists.
- `map.keys() -> list[string]`: Returns list of all keys.
- `map.values() -> list[T]`: Returns list of all values.
- `map.len() -> int`: Returns number of key-value pairs.
- `map.remove(key: string) -> T`: Removes key and returns associated value.

---

## 3. The `math` Module

Import via `import math` or `from math import sqrt, pi`.

### 3.1 Constants
- `math.pi`: Mathematical constant $\pi \approx 3.141592653589793$.
- `math.e`: Euler's number $e \approx 2.718281828459045$.

### 3.2 Functions
- `math.sqrt(x: float) -> float`: Square root.
- `math.pow(base: float, exp: float) -> float`: Power calculation.
- `math.sin(x: float) -> float`: Sine (radians).
- `math.cos(x: float) -> float`: Cosine (radians).
- `math.tan(x: float) -> float`: Tangent (radians).
- `math.abs(x: float | int) -> float | int`: Absolute value.
- `math.log(x: float) -> float`: Natural logarithm.
- `math.exp(x: float) -> float`: Natural exponential ($e^x$).
- `math.floor(x: float) -> int`: Floor value.
- `math.ceil(x: float) -> int`: Ceiling value.
- `math.round(x: float) -> int`: Nearest integer.

---

## 4. The `data` Module

Import via `import data`.

### 4.1 Tabular Data Structures
- `data.load(path: string) -> DataFrame`: Loads CSV data from disk with delimiter detection.
- `data.from_csv(csv_content: string) -> DataFrame`: Parses CSV directly from in-memory string.

### 4.2 DataFrame Methods
- `df.rows() -> int`: Number of rows.
- `df.cols() -> int`: Number of columns.
- `df.column_names() -> list[string]`: List of column header names.
- `df.clean() -> DataFrame`: Drops null/NaN rows and trims whitespace.
- `df.shuffle() -> DataFrame`: Shuffles rows pseudo-randomly.
- `df.head(n: int = 5) -> DataFrame`: Returns top `n` rows.
- `df.describe() -> map[string, map[string, float]]`: Computes mean, min, max, std dev for numerical columns.
- `df.split(train_ratio: float = 0.8) -> list[DataFrame]`: Splits dataset into `[train_df, test_df]`.

---

## 5. The `tensor` & `ai` Modules

Import via `import tensor` and `import ai`.

### 5.1 Tensor API (`tensor`)
- `tensor.tensor(data: list) -> Tensor`: Creates multi-dimensional tensor from nested numeric array.
- `tensor.zeros(shape: list[int]) -> Tensor`: Initializes tensor with zeros.
- `tensor.ones(shape: list[int]) -> Tensor`: Initializes tensor with ones.
- `tensor.randn(shape: list[int]) -> Tensor`: Normal Gaussian random initialization.
- `tensor.matmul(a: Tensor, b: Tensor) -> Tensor`: High-performance 2D matrix multiplication.
- `tensor.add(a: Tensor, b: Tensor) -> Tensor`: Element-wise tensor addition.
- `tensor.sub(a: Tensor, b: Tensor) -> Tensor`: Element-wise tensor subtraction.
- `tensor.mul(a: Tensor, b: Tensor) -> Tensor`: Element-wise tensor multiplication.
- `tensor.relu(x: Tensor) -> Tensor`: Rectified Linear Unit activation.
- `tensor.sigmoid(x: Tensor) -> Tensor`: Sigmoid activation.

### 5.2 AI Model API (`ai`)
- `ai.load(model_path: string) -> Model`: Deserializes saved model weights and architecture.
- `ai.create_linear(in_features: int, out_features: int) -> Model`: Instantiates dense linear neural layer.
- `model.predict(input: Tensor | DataFrame) -> Tensor`: Runs forward pass inference.
- `model.train(dataset: DataFrame, epochs: int, lr: float) -> map`: Trains model parameters using backpropagation.
- `model.save(path: string) -> bool`: Serializes model weights to disk.

---

## 6. The `sys` Module

Import via `import sys`.

- `sys.version`: NextViper runtime version string (e.g., `"1.0.0"`).
- `sys.platform`: Host OS platform (`"linux"`, `"macos"`, or `"windows"`).
- `sys.args`: Command-line arguments passed to script.
- `sys.read_file(path: string) -> string`: Safely reads file contents into string.
- `sys.write_file(path: string, content: string) -> bool`: Writes string content to file.
- `sys.time_ns() -> int`: Monotonic timestamp in nanoseconds.
- `sys.exit(code: int = 0) -> nil`: Terminates process execution with status code.
