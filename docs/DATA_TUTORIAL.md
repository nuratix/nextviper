# NextViper Data Subsystem Tutorial: From CSV to Model-Ready Data

In this tutorial, you will learn how to load a dataset, inspect its structure, clean missing values, transform numeric features, and prepare train/test splits for training.

---

## 1. Loading and Inspecting a Dataset

Create a sample CSV file `housing.csv`:

```csv
rooms,sqft,age,price
3,1200.0,10,250000.0
4,1850.0,5,380000.0
2,850.0,null,175000.0
5,2400.0,2,490000.0
3,1350.0,15,270000.0
```

Load the CSV into NextViper:

```nextviper
import data
import std.io

// 1. Load the dataset
let df = data.load("housing.csv")

// 2. Inspect dimensions and columns
io.print("Shape (rows, cols):", df.shape)
io.print("Columns:", df.columns)

// 3. View the first 3 rows
let preview = df.head(3)
io.print(preview.to_csv())
```

Output:
```
Shape (rows, cols): [5, 4]
Columns: ["rooms", "sqft", "age", "price"]
rooms,sqft,age,price
3,1200,10,250000
4,1850,5,380000
2,850,,175000
```

---

## 2. Cleaning Missing Values

Real-world datasets often contain missing (`null`) entries. You can clean them with `.drop_missing()`:

```nextviper
// Remove all rows with missing values
let clean_df = df.drop_missing()
io.print("Clean rows count:", clean_df.num_rows)
```

Output:
```
Clean rows count: 4
```

---

## 3. Summary Statistics

Generate statistical descriptions (`count`, `mean`, `min`, `max`, `std`):

```nextviper
let stats = clean_df.describe()
io.print("Mean square footage:", stats["sqft"]["mean"])
io.print("Max price:", stats["price"]["max"])
```

---

## 4. Feature Normalization & Splitting

Before training machine learning models, features should be normalized and split into training and test partitions:

```nextviper
// 1. Min-Max normalize numeric columns to [0.0, 1.0]
let normalized_df = clean_df.normalize()

// 2. Deterministically shuffle data with seed
let shuffled_df = normalized_df.shuffle(42)

// 3. Split into 80% train and 20% test sets
let split_pair = shuffled_df.split(0.8, 42)
let train_set = split_pair[0]
let test_set = split_pair[1]

io.print("Training set rows:", train_set.num_rows)
io.print("Testing set rows:", test_set.num_rows)
```

---

## 5. Converting to Tensors

Convert preprocessed features into `Tensor` instances ready for neural networks:

```nextviper
let feature_cols = ["rooms", "sqft", "age"]
let train_tensors = train_set.to_tensor(feature_cols)

io.print("Tensor Shape:", train_tensors.shape)
```

Output:
```
Tensor Shape: [3, 3]
```

You are now ready to feed your preprocessed data directly into the NextViper AI and neural network pipelines!
