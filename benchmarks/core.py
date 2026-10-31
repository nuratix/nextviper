#!/usr/bin/env python3
"""Small correctness-checked wall-clock benchmarks; includes process startup."""
import argparse
import json
import platform
import statistics
import subprocess
import tempfile
import time
from pathlib import Path

CASES = {
    'startup': ('print(1)\n', '1\n'),
    'calls': ('fn inc(x) => x + 1\nlet mut n = 0\nfor i in 0..1000:\n    n = inc(n)\nprint(n)\n', '1000\n'),
    'loops': ('let mut n = 0\nfor i in 0..10000:\n    n += 1\nprint(n)\n', '10000\n'),
    'recursion': ('fn fib(n):\n    if n < 2:\n        return n\n    return fib(n - 1) + fib(n - 2)\nprint(fib(14))\n', '377\n'),
    'strings': ('let mut s = ""\nfor i in 0..1000:\n    s += "x"\nprint(len(s))\n', '1000\n'),
    'collections': ('let a = []\nfor i in 0..1000:\n    push(a, i)\nprint(len(a))\n', '1000\n'),
    'allocation': ('let a = []\nfor i in 0..500:\n    push(a, [i, i + 1])\nprint(len(a))\n', '500\n'),
}


def measure(command, repeats, expected=None):
    samples = []
    for iteration in range(repeats + 1):
        start = time.perf_counter()
        result = subprocess.run(command, capture_output=True, text=True, timeout=90)
        elapsed = (time.perf_counter() - start) * 1000
        if result.returncode or (expected is not None and (result.stdout != expected or result.stderr)):
            raise RuntimeError(f'{command}: exit={result.returncode}, stdout={result.stdout!r}, stderr={result.stderr!r}')
        if iteration:  # one excluded warmup
            samples.append(elapsed)
    return {'samples_ms': samples, 'median_ms': statistics.median(samples),
            'min_ms': min(samples), 'max_ms': max(samples)}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--binary', type=Path, default=Path('bin/nextviper'))
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--label', required=True)
    parser.add_argument('--repeat', type=int, default=3)
    args = parser.parse_args()
    if args.repeat < 1:
        parser.error('--repeat must be positive')
    binary = args.binary.resolve()
    report = {'label': args.label, 'platform': platform.platform(), 'machine': platform.machine(),
              'python': platform.python_version(), 'repeats': args.repeat,
              'method': 'wall clock, one warmup, includes startup; no competitive language comparison',
              'results': {}}
    with tempfile.TemporaryDirectory(prefix='nv-core-bench-') as directory:
        work = Path(directory)
        for name, (source, expected) in CASES.items():
            path = work / (name + '.nv')
            path.write_text(source)
            report['results'][name] = measure([str(binary), 'run', str(path), '--tree'], args.repeat, expected)
        executable = work / 'native'
        report['results']['compilation'] = measure([str(binary), 'build', str(work / 'startup.nv'), '-o', str(executable)], args.repeat)
        report['results']['native_startup'] = measure([str(executable)], args.repeat, '1\n')
    args.output.write_text(json.dumps(report, indent=2) + '\n')
    for name, result in report['results'].items():
        print(f'{name}: {result["median_ms"]:.3f} ms median')


if __name__ == '__main__':
    main()
