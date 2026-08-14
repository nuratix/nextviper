# Benchmark 07: File Processing - Loading CSV dataset, cleaning, filtering, and summary metrics in Python
import csv
import random

with open("benchmarks/sample_data.csv", "r") as f:
    reader = csv.reader(f)
    header = next(reader)
    rows = []
    for r in reader:
        if r and all(field.strip() != "" for field in r):
            rows.append(r)

random.seed(42)
random.shuffle(rows)

split_idx = int(len(rows) * 0.8)
train_set = rows[:split_idx]
test_set = rows[split_idx:]

print(len(train_set) + len(test_set))
