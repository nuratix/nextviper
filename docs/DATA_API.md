# NextViper Data Subsystem API Reference

## Module: `data`

The `data` module provides utilities for creating, loading, inspecting, transforming, and processing tabular data and numerical arrays.

---

## 1. Top-Level Functions

### `data.load(path: string) -> DataFrame`
Loads a dataset from disk, automatically detecting format by file extension (`.csv`, `.json`, `.jsonl`).

```nextviper
import data

let df = data.load("users.csv")
```

### `data.read_csv(content_or_path: string) -> DataFrame`
Parses CSV content from a string or reads a CSV file from a path.

```nextviper
let df = data.read_csv("name,age\nAlice,30\nBob,25\n")
```

### `data.read_json(content_or_path: string) -> DataFrame`
Parses JSON array of objects or line-delimited JSON (JSONL).

```nextviper
let df = data.read_json("[{\"name\": \"Alice\", \"age\": 30}]")
```

### `data.array(values: list[float|int]) -> DataArray`
Creates a numerical array from a list of numbers.

```nextviper
let arr = data.array([1.0, 2.0, 3.0, 4.0, 5.0])
```

### `data.zeros(shape: list[int]|int) -> DataArray`
Creates an array of zeros with the specified shape.

```nextviper
let z = data.zeros([100, 4])
```

### `data.ones(shape: list[int]|int) -> DataArray`
Creates an array of ones with the specified shape.

```nextviper
let o = data.ones([10, 10])
```

### `data.arange(start: float, stop: float, step: float = 1.0) -> DataArray`
Generates evenly spaced values within a given half-open interval `[start, stop)`.

```nextviper
let r = data.arange(0.0, 10.0, 0.5)
```

### `data.linspace(start: float, stop: float, num: int = 50) -> DataArray`
Generates `num` evenly spaced numbers over the closed interval `[start, stop]`.

```nextviper
let grid = data.linspace(0.0, 1.0, 100)
```

---

## 2. `DataFrame` Methods

| Method | Return Type | Description |
| :--- | :--- | :--- |
| `df.columns` | `list[string]` | List of column names in the table |
| `df.shape` | `list[int]` | `[num_rows, num_cols]` dimensions |
| `df.num_rows` | `int` | Total number of rows |
| `df.num_cols` | `int` | Total number of columns |
| `df.select(columns: list[string])` | `DataFrame` | Projects a subset of columns |
| `df.drop(columns: list[string])` | `DataFrame` | Drops specified columns |
| `df.head(n: int = 5)` | `DataFrame` | Returns the first `n` rows |
| `df.tail(n: int = 5)` | `DataFrame` | Returns the last `n` rows |
| `df.sort(column: string, asc: bool = true)`| `DataFrame` | Sorts rows by column value |
| `df.clean(drop_nulls: bool = true)` | `DataFrame` | Cleans missing values |
| `df.drop_missing()` | `DataFrame` | Drops any rows containing nulls |
| `df.shuffle(seed: int = 42)` | `DataFrame` | Deterministically shuffles rows |
| `df.split(train_ratio: float, seed: int)` | `list[DataFrame]` | Splits into `[train_df, test_df]` |
| `df.normalize()` | `DataFrame` | Min-max normalizes numeric columns to `[0, 1]` |
| `df.standardize()` | `DataFrame` | Standardizes numeric columns to mean=0, std=1 |
| `df.describe()` | `map[string, map]` | Returns summary statistics (`count`, `mean`, `min`, `max`, `std`, `nulls`) |
| `df.to_tensor(columns = [])` | `Tensor` | Converts numeric columns into a 2D `Tensor` |
| `df.to_array(column: string)` | `DataArray` | Converts a single column into a `DataArray` |
| `df.to_csv()` | `string` | Serializes DataFrame into CSV formatted text |
| `df.to_json()` | `string` | Serializes DataFrame into JSON array of objects |

---

## 3. `DataArray` Methods

| Method | Return Type | Description |
| :--- | :--- | :--- |
| `arr.shape` | `list[int]` | Dimensions of the array |
| `arr.size` | `int` | Total number of elements |
| `arr.mean()` | `float` | Arithmetic mean of elements |
| `arr.sum()` | `float` | Total sum of elements |
| `arr.min()` | `float` | Minimum element value |
| `arr.max()` | `float` | Maximum element value |
| `arr.std()` | `float` | Standard deviation |
| `arr.var()` | `float` | Variance |
| `arr.median()` | `float` | Median value |
| `arr.normalize(min=0.0, max=1.0)` | `DataArray` | Scales array elements into `[min, max]` |
| `arr.standardize()` | `DataArray` | Normalizes to zero mean and unit variance |
| `arr.to_tensor()` | `Tensor` | Zero-copy conversion into a `Tensor` |
