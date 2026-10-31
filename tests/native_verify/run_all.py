#!/usr/bin/env python3
"""Exact native/interpreter stdout, stderr and exit status comparisons.

Only the elapsed-time line in the Fibonacci example is normalized; its label
and numeric format are still checked. No other output is discarded.
"""
import os
from pathlib import Path
import re
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
BIN = Path(os.environ.get('NEXTVIPER_BIN', ROOT / 'bin/nextviper')).resolve()
FIXTURES = sorted(Path(__file__).parent.glob('*.nv')) + [ROOT / 'examples/fibonacci.nv']


def observable(result, name):
    stdout = result.stdout
    if name == 'fibonacci.nv':
        stdout, count = re.subn(r'^Elapsed time \(sec\): [0-9.eE+\-]+$',
                               'Elapsed time (sec): <measured>', stdout, flags=re.MULTILINE)
        if count != 1:
            raise AssertionError('missing or malformed Fibonacci elapsed-time line')
    return result.returncode, stdout, result.stderr


class NativeEquivalence(unittest.TestCase):
    def test_fixtures(self):
        for path in FIXTURES:
            with self.subTest(program=path.name), tempfile.TemporaryDirectory(prefix='nv-native-verify-') as directory:
                binary = Path(directory) / 'program'
                interpreted = subprocess.run([str(BIN), 'run', str(path), '--tree'],
                                             capture_output=True, text=True, timeout=30)
                self.assertEqual(interpreted.returncode, 0, interpreted.stderr)
                built = subprocess.run([str(BIN), 'build', '--native', str(path), '-o', str(binary)],
                                       capture_output=True, text=True, timeout=90)
                self.assertEqual(built.returncode, 0, built.stderr)
                native = subprocess.run([str(binary)], capture_output=True, text=True, timeout=30)
                self.assertEqual(observable(native, path.name), observable(interpreted, path.name))
                print(f'PASS: {path.name}', flush=True)


if __name__ == '__main__':
    print(f'Native equivalence: {len(FIXTURES)} fixtures', flush=True)
    unittest.main(verbosity=2)
