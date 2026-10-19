# Performance measurement

Previous comparative speed tables are withdrawn. They were not established as reliable measurements of the current implementation. In particular, the earlier benchmark harness labelled two interpreter invocations as different execution engines.

The harness now uses `run --tree` for the interpreter and `run --vm` for the in-memory VM. Native compilation generates C++20 with tagged dynamic values. Its performance must be measured again after correctness changes; previous numbers do not apply.

```sh
make
python3 benchmarks/harness.py
```

The harness writes local timings to `benchmarks/results.json`. It includes process startup costs, does not provide a general language ranking, and is not a production capacity test. Check outputs for equivalence before interpreting timings. Report hardware, OS, compiler flags, language/runtime versions, workload, warm-up, repetitions, variance, and failures. Compare AI/data workloads against appropriate optimized libraries, not only handwritten Python loops.

No general speed advantage over Python, Go, Rust, or C++ is claimed.
