# NextViper Data Subsystem Architecture

The **NextViper Data Subsystem** provides a zero-copy, high-performance library and runtime layer for numerical computing, tabular analysis, data preprocessing, and AI pipelines without polluting core language syntax.

---

## 1. Architectural Philosophy

1. **Library-Layer Design**: All data functionality is implemented as a modular library in NextViper (`data.*` and `data/` modules), preserving the cleanliness of the core language syntax.
2. **Columnar & Continuous Memory**:
   - `DataArray`: Contiguous `std::vector<double>` storage with shape metadata, supporting vectorized operations, reductions, and tensor interoperability.
   - `DataFrame`: Column-oriented schema with typed columns (`int64`, `float64`, `string`, `bool`, `null`).
3. **Format Extensibility**: Designed with format abstraction layers supporting CSV, JSON/JSONL, and future Apache Parquet zero-copy Arrow readers.
4. **AI Pipeline Interoperability**: Direct zero-copy conversion between `DataFrame` / `DataArray` and NextViper's `Tensor` and `DataLoader` abstractions.

---

## 2. Core Abstractions

```
                                  ┌─────────────┐
                                  │   Schema    │
                                  └──────┬──────┘
                                         │
┌──────────────┐     ┌──────────────┐    │     ┌──────────────┐
│  DataArray   │◄───►│  DataFrame   │◄───┴────►│    Column    │
└──────┬───────┘     └──────┬───────┘          └──────────────┘
       │                    │
       ▼                    ▼
┌──────────────┐     ┌──────────────┐
│    Tensor    │◄────┤   Dataset    │
└──────────────┘     └──────┬───────┘
                            │
                            ▼
                     ┌──────────────┐
                     │  DataLoader  │
                     └──────────────┘
```

### 1. `DataArray`
- 1D/ND contiguous array of 64-bit floating-point or integer elements.
- Vectorized SIMD-ready operations: `add`, `sub`, `mul`, `div`, `pow`, `abs`, `exp`, `log`, `sqrt`, `cumsum`.
- Statistical reductions: `mean`, `sum`, `min`, `max`, `std`, `var`, `median`, `argmax`, `argmin`.
- In-place and non-destructive transformations: `normalize`, `standardize`, `clip`, `reshape`.

### 2. `DataFrame`
- Tabular 2D data structure with named headers, typed schema, and row-level / column-level access.
- Fast I/O: RFC 4180 CSV parser and JSON record reader.
- Relational and query operations: `select`, `drop`, `filter`, `sort`, `slice`, `head`, `tail`.
- Preprocessing: `clean`, `drop_missing`, `fill_missing`, `normalize`, `standardize`, `shuffle`.
- Data splitting & mini-batching: `split(train_ratio, seed)`, `sample(n)`, `sample_fraction(frac)`, `batches(batch_size)`.

### 3. `Schema` & `Column`
- Type validation supporting `INT64`, `FLOAT64`, `STRING`, `BOOL`, `NULL_VAL`.
- Null tracking for robust missing value handling without sentinel crashes.

---

## 3. Performance & Memory Efficiency

- **Direct Native Memory Layout**: High-volume numeric arrays avoid object overhead and boxed value pointer chasing.
- **Lazy and In-Place Transforms**: Normalization and standardization compute min/max/mean/stddev in single-pass linear scans.
- **Deterministic Garbage Collection**: Reference counting (`std::shared_ptr`) ensures data structures are released immediately upon leaving scope.
- **Multithreading & SIMD Preparedness**: Contiguous memory blocks enable auto-vectorization by compiler backends and OpenMP / pthread parallel reductions.
