#!/usr/bin/env python3
"""Observable language behavior shared by the supported execution paths."""
import os
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
BIN = Path(os.environ.get('NEXTVIPER_BIN', ROOT / 'bin/nextviper')).resolve()


class LanguageContract(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory(prefix='nv-contract-')
        self.addCleanup(self.directory.cleanup)
        self.work = Path(self.directory.name)

    def execute(self, source, engine='tree'):
        path = self.work / 'program.nv'
        path.write_text(source)
        if engine == 'native':
            binary = self.work / 'program'
            built = subprocess.run([str(BIN), 'build', str(path), '-o', str(binary)],
                                   capture_output=True, text=True, timeout=60)
            self.assertEqual(built.returncode, 0, built.stderr)
            command = [str(binary)]
        else:
            command = [str(BIN), 'run', str(path), '--' + engine]
        return subprocess.run(command, capture_output=True, text=True, timeout=15)

    def assert_output(self, source, expected, engines=('tree', 'vm', 'native')):
        for engine in engines:
            with self.subTest(engine=engine):
                result = self.execute(source, engine)
                self.assertEqual(result.returncode, 0, result.stderr)
                self.assertEqual(result.stdout, expected)

    def test_integer_boundaries_and_precise_comparisons(self):
        self.assert_output('print(9223372036854775807 > 9223372036854775806)\n'
                           'print(9223372036854775807 == 9223372036854775806)\n'
                           'print((-9223372036854775807 - 1) % -1)\n'
                           'print(1 ** 9223372036854775807)\n', 'true\nfalse\n0\n1\n')

    def test_overflow_fails_instead_of_wrapping_or_crashing(self):
        for expression in ('9223372036854775807 + 1', '-9223372036854775807 - 2',
                           '9223372036854775807 * 2', '2 ** 63',
                           '-(-9223372036854775807 - 1)',
                           '(-9223372036854775807 - 1) / -1',
                           'abs(-9223372036854775807 - 1)', 'pow(2, 63)'):
            for engine in ('tree', 'vm', 'native'):
                with self.subTest(expression=expression, engine=engine):
                    result = self.execute('print(' + expression + ')\n', engine)
                    self.assertEqual(result.returncode, 1, result.stderr)
                    self.assertIn('overflow', result.stderr.lower())

    def test_compound_assignments_match_binary_operators(self):
        self.assert_output('let a = [20, 7.5, "a", [1]]\n'
                           'print(a[0] *= 3)\na[0] /= 4\na[0] %= 4\n'
                           'a[1] -= 2.5\na[2] += "b"\na[3] += [2]\n'
                           'let o = {"n": 12}\no["n"] /= 3\no["n"] += 2\n'
                           'print(a)\nprint(o["n"])\n',
                           '60\n[3, 5.0, "ab", [1, 2]]\n6\n', engines=('tree', 'vm'))

    def test_assignment_target_is_evaluated_once_before_value(self):
        self.assert_output('let a = [10]\nlet events = []\n'
                           'fn target():\n    push(events, "target")\n    return a\n'
                           'fn index():\n    push(events, "index")\n    return 0\n'
                           'fn value():\n    push(events, "value")\n    return 2\n'
                           'target()[index()] += value()\nprint(a)\nprint(events)\n',
                           '[12]\n["target", "index", "value"]\n', engines=('tree', 'vm'))

    def test_break_and_continue_unwind_nested_locals(self):
        self.assert_output('let mut total = 0\nfor i in [1, 2, 3, 4]:\n'
                           '    let scratch = [i]\n    if i == 2:\n        let x = 99\n        continue\n'
                           '    if i == 4:\n        let x = 88\n        break\n    total += scratch[0]\n'
                           'let mut n = 0\nwhile n < 3:\n    let x = n\n    n += 1\n'
                           '    if n == 2:\n        continue\n    total += x\n'
                           'print(total)\n', '6\n')

    def test_inclusive_range_expression_and_maximum_endpoint(self):
        self.assert_output('let a = 2..=4\nprint(a)\n'
                           'for i in 9223372036854775806..=9223372036854775807:\n    print(i)\n',
                           '[2, 3, 4]\n9223372036854775806\n9223372036854775807\n',
                           engines=('tree', 'vm'))

    def test_mutating_collection_during_iteration_is_defined(self):
        self.assert_output('let a = [1, 2, 3]\nlet seen = []\nfor x in a:\n'
                           '    push(seen, x)\n    push(a, 9)\nprint(seen)\nprint(len(a))\n',
                           '[1, 2, 3]\n6\n', engines=('tree',))

    def test_invalid_arithmetic_and_indices_are_errors(self):
        for source in ('print("x" / 2)', 'print(5.5 % 2)', 'print([1]["wrong"])',
                       'let a = [1]\na["wrong"] = 2', 'let a = {}\na[true] = 2',
                       'let a = {}\na["missing"] += 1', 'print(int(1e100))'):
            for engine in ('tree', 'vm'):
                with self.subTest(source=source, engine=engine):
                    result = self.execute(source, engine)
                    self.assertEqual(result.returncode, 1, result.stderr)

    def test_vm_limits_fail_explicitly(self):
        programs = ['let a = [' + ','.join('1' for _ in range(256)) + ']\n',
                    '\n'.join('let v' + str(i) + ' = 1' for i in range(200))]
        for source in programs:
            result = self.execute(source, 'vm')
            self.assertEqual(result.returncode, 1, result.stderr)
            self.assertTrue('limit' in result.stderr or 'too many' in result.stderr, result.stderr)

    def test_vm_unsupported_features_are_diagnosed(self):
        for source in ('import math\nprint(math.sqrt(4))\n',
                       'fn outer(x):\n    return fn(y) => x + y\nprint(outer(1)(2))\n'):
            result = self.execute(source, 'vm')
            self.assertEqual(result.returncode, 1, result.stderr)
            self.assertIn('interpreter', result.stderr.lower())

    def test_invalid_function_control_flow_is_an_error(self):
        for keyword in ('break', 'continue'):
            result = self.execute('fn invalid():\n    ' + keyword + '\nfor i in 0..2:\n    invalid()\n')
            self.assertEqual(result.returncode, 1, result.stderr)
            self.assertIn('outside loop', result.stderr)

    def test_failed_module_initialization_does_not_export_partial_values(self):
        (self.work / 'broken.nv').write_text('let value = 42\nassert(false, "module failed")\n')
        result = self.execute('import broken\nprint(broken.value)\n')
        self.assertEqual(result.returncode, 1, result.stderr)
        self.assertNotIn('42', result.stdout)


if __name__ == '__main__':
    unittest.main(verbosity=2)
