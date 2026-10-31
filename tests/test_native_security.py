#!/usr/bin/env python3
"""Native output paths are passed as argv, including spaces/metacharacters."""
import os
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
BIN = Path(os.environ.get('NEXTVIPER_BIN', ROOT / 'bin/nextviper')).resolve()


class NativePathSecurity(unittest.TestCase):
    def test_output_paths_are_not_shell_commands(self):
        with tempfile.TemporaryDirectory(prefix='nv-native-security-') as directory:
            work = Path(directory)
            source = work / 'source.nv'
            source.write_text('print("SAFE_EXECUTION_SUCCESS_42")\n')
            marker = work / 'INJECTION_MARKER'
            for name in ('my compiled binary', 'prog;touch INJECTION_MARKER;', 'prog$(touch INJECTION_MARKER)'):
                with self.subTest(path=name):
                    binary = work / name
                    built = subprocess.run([str(BIN), 'build', '--native', str(source), '-o', str(binary)],
                                           capture_output=True, text=True, timeout=90, cwd=work)
                    self.assertEqual(built.returncode, 0, built.stderr)
                    self.assertFalse(marker.exists(), 'output path executed a shell command')
                    result = subprocess.run([str(binary)], capture_output=True, text=True, timeout=10, cwd=work)
                    self.assertEqual((result.returncode, result.stdout, result.stderr),
                                     (0, 'SAFE_EXECUTION_SUCCESS_42\n', ''))
                    self.assertFalse(marker.exists())


if __name__ == '__main__':
    unittest.main(verbosity=2)
