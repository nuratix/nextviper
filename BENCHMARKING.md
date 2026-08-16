# NextViper High-Throughput Benchmarking System

NextViper includes `nextviper bench` to evaluate and compare performance across execution runtimes.

---

## 1. Overview

`nextviper bench` executes source code across 100+ iterations, measuring latency and throughput under:
1. **Tree-Walk Interpreter**
2. **Bytecode Virtual Machine**
3. **Native AOT Compiled Binary**

---

## 2. Usage

```bash
nextviper bench benchmarks/matrix_bench.nv
```

### Sample Output
```
====================================================
  NextViper High-Performance Benchmark Suite
  Target: benchmarks/matrix_bench.nv
====================================================

Execution Latency (average across 100 iterations):
  1. AST Tree-Walk Interpreter: 0.1240 ms / run
  2. NextViper Bytecode VM:     0.0380 ms / run
  3. Native Machine Code:       0.0031 ms / run

Speedup Comparison:
  • Native vs Interpreter: 40.0x faster
  • Native vs Bytecode VM: 12.2x faster
====================================================
```
