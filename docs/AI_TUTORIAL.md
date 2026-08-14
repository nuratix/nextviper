# Learn NextViper AI in 10 Steps: Complete Beginner Tutorial

Welcome to machine learning with NextViper! In this tutorial, you will build, train, evaluate, save, and deploy a neural network from scratch using standard NextViper code.

---

## Step 1: Loading and Inspecting Data

NextViper provides a robust `data` subsystem to read structured CSV and tabular datasets:

```nextviper
import data

// Load tabular training dataset
let raw_csv = "feature1,feature2,target\n0.0,0.0,0.0\n0.0,1.0,1.0\n1.0,0.0,1.0\n1.0,1.0,0.0\n"
let df = data.read_csv(raw_csv)

print("Rows:", df.num_rows)
print("Columns:", df.columns)
```

---

## Step 2: Converting Data to Tensors

Machine learning models operate on multidimensional arrays called **Tensors**. Convert DataFrame columns into input tensors ($X$) and target tensors ($Y$):

```nextviper
import tensor

// Extract input features and target labels
let x_train = df.to_tensor(["feature1", "feature2"])
let y_train = df.to_tensor(["target"])

print("X Tensor Shape:", x_train.shape) // [4, 2]
print("Y Tensor Shape:", y_train.shape) // [4, 1]
```

---

## Step 3: Building Neural Network Architecture

Construct a multi-layer perceptron using `ai.Sequential` and `ai.Dense`:

```nextviper
import ai

let model = ai.Sequential([
    ai.Dense(2, 8, "relu"),    // Input Layer (2 features) -> Hidden Layer (8 units, ReLU)
    ai.Dense(8, 1, "sigmoid")  // Hidden Layer (8 units) -> Output Layer (1 probability, Sigmoid)
])

model.summary()
```

---

## Step 4: Configuring the Loss Function

The loss function measures the difference between model predictions and true targets:

```nextviper
// Use Mean Squared Error (or ai.BCE() for binary classification)
let loss_fn = ai.MSE()
```

---

## Step 5: Choosing and Configuring an Optimizer

The optimizer adjusts model weights using computed gradients during backpropagation:

```nextviper
// Adam optimizer with learning rate 0.05
let optimizer = ai.Adam(0.05)
```

---

## Step 6: Compiling the Model

Bind the optimizer and loss function to the model:

```nextviper
model.compile(optimizer, loss_fn)
```

---

## Step 7: Training the Model (`model.fit`)

Train the network over 200 epochs using mini-batches:

```nextviper
print("Starting training...")
let history = model.fit(x_train, y_train, 200, 4)

print("Epochs completed:", history.epochs)
```

---

## Step 8: Evaluating Model Performance

Evaluate your model on test data to inspect accuracy and loss:

```nextviper
let eval_results = model.evaluate(x_train, y_train)
print("Evaluation Loss:", eval_results["loss"])
```

---

## Step 9: Safe Model Serialization

Save your trained model to a safe `.nvmodel` file on disk:

```nextviper
let model_path = "xor_classifier.nvmodel"
model.save(model_path)

print("Model saved to:", model_path)
```

---

## Step 10: Loading the Model & Running Live Inference

Load the model in your production environment and run predictions on new inputs:

```nextviper
// Restore model from disk
let loaded_model = ai.load("xor_classifier.nvmodel")

// Run inference on new data
let sample_input = tensor.from([[0.0, 1.0], [1.0, 1.0]])
let predictions = loaded_model.predict(sample_input)

print("Prediction for [0, 1] (expect ~1.0):", predictions.get(0, 0))
print("Prediction for [1, 1] (expect ~0.0):", predictions.get(1, 0))
```

Congratulations! You have completed the 10-step NextViper AI tutorial.
