import random

# Generate a 2000-row CSV file for benchmarking
with open("benchmarks/sample_data.csv", "w") as f:
    f.write("id,feature_a,feature_b,score,category\n")
    for i in range(2000):
        fa = round(random.uniform(0.0, 100.0), 2)
        fb = round(random.uniform(10.0, 500.0), 2)
        sc = round(fa * 0.4 + fb * 0.6, 2)
        cat = f"cat_{i % 5}"
        f.write(f"{i},{fa},{fb},{sc},{cat}\n")
print("Generated benchmarks/sample_data.csv")
