#!/usr/bin/env python3
"""Behavior regressions for the independently reproduced readiness failures."""
import json
import os
from pathlib import Path
import resource
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
BIN = Path(os.environ.get('NEXTVIPER_BIN', ROOT / 'bin/nextviper')).resolve()


def subprocess_limits():
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))


class ReadinessRegressions(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='nextviper-regression-')
        self.work = Path(self.temp.name)
        self.addCleanup(self.temp.cleanup)

    def invoke(self, *args, **kwargs):
        return subprocess.run([str(BIN), *map(str, args)], cwd=self.work,
                              capture_output=True, text=True, timeout=30,
                              preexec_fn=subprocess_limits, **kwargs)

    def source(self, text, name='test.nv'):
        path = self.work / name
        path.write_text(text)
        return path

    def native_equivalent(self, text):
        source = self.source(text)
        interpreted = self.invoke('run', source)
        self.assertEqual(interpreted.returncode, 0, interpreted.stderr)
        target = self.work / 'program'
        built = self.invoke('build', '--native', source, '-o', target)
        self.assertEqual(built.returncode, 0, built.stderr)
        native = subprocess.run([str(target)], capture_output=True, text=True,
                                timeout=10, preexec_fn=subprocess_limits)
        self.assertEqual(native.returncode, 0, native.stderr)
        self.assertEqual(native.stdout, interpreted.stdout)
        return native.stdout

    def test_fractional_functions_and_math(self):
        out = self.native_equivalent('fn twice(x) => x * 2\nprint(twice(1.75))\nprint(sqrt(2.0))\nprint(5 / 2)\nprint(4 / 2)\n')
        self.assertEqual(out, '3.5\n1.41421\n2.5\n2\n')

    def test_native_tagged_collections(self):
        out = self.native_equivalent('print({"count": 100000, "negative": -200000, "flag": true, "empty": nil, "fraction": 1.75})\nprint(["hello", true, nil, 100000, 1.75])\nprint("hello" + "world")\n')
        self.assertIn('100000', out)
        self.assertIn('1.75', out)

    def test_captured_closures(self):
        out = self.native_equivalent('fn make_adder(bias):\n    return fn(x) => x + bias\nlet add = make_adder(10)\nprint(add(5))\n')
        self.assertEqual(out, '15\n')

    def test_mutable_closure_cells(self):
        out = self.native_equivalent('fn counter():\n    let mut count = 0\n    fn inc():\n        count += 1\n        return count\n    return inc\nlet next = counter()\nprint(next())\nprint(next())\n')
        self.assertEqual(out, '1\n2\n')

    def test_native_control_flow_and_ranges(self):
        out = self.native_equivalent('print(2 ** 8)\nprint(false and (1 / 0))\nprint(true or (1 / 0))\nprint(0 or 42)\nprint(\"\" and 99)\nfor i in range(5, 0, -2):\n    print(i)\nlet i = 99\nfor i in 0..2:\n    print(i)\nprint(i)\nlet x = 1\nif true:\n    let x = 2\n    print(x)\nprint(x)\n')
        self.assertEqual(out, '256\nfalse\ntrue\n42\n\n5\n3\n1\n0\n1\n99\n2\n1\n')

    def test_modulo_assignment(self):
        self.assertEqual(self.native_equivalent('let mut x = 10\nx %= 3\nprint(x)\n'), '1\n')
        result = self.invoke('eval', 'let mut x = 10; x %= 0')
        self.assertEqual(result.returncode, 1)

    def test_immutable_reassignment_rejected(self):
        source = self.source('let x = 1\nx = 2\n')
        for command in ('run', 'check', 'build', 'compile'):
            result = self.invoke(command, source)
            self.assertEqual(result.returncode, 1, (command, result.stdout, result.stderr))

    def test_primitive_annotations_enforced_at_runtime(self):
        for text in ('let mut x: int = 1\nx = "oops"\n', 'let x: int = "oops"\n'):
            source = self.source(text)
            for command in ('run', 'check'):
                self.assertEqual(self.invoke(command, source).returncode, 1)

    def test_invalid_dataframe_inputs_are_errors(self):
        for call in ('data.from_rows(1, 2)', 'data.from_rows(["a"], [1])', 'data.from_rows([1], [])', 'data.from_rows(["a"], [[1, 2]])'):
            result = self.invoke('eval', 'import data\n' + call)
            self.assertEqual(result.returncode, 1, result.stderr)
            self.assertIn('error', result.stderr)

    def test_cycles_print_without_crashing(self):
        result = self.invoke('eval', 'let a = []\npush(a, a)\nprint(a)\n')
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(result.stdout, '[<cycle>]\n')
        self.assertEqual(self.native_equivalent('let a = []\npush(a, a)\nprint(a)\n'), '[<cycle>]\n')

    def test_formatter_preserves_programs(self):
        programs = ['print(2 ** 3)\n', 'fn square(x) => x * x\nprint(square(4))\n',
                    'fn f():\n    print("inside")\nprint("outside")\nf()\n',
                    'let mut x = 10\nx %= 3\nprint(x)\n',
                    'print(1e-3)\nprint("a // b")\n']
        for text in programs:
            path = self.source(text)
            before = self.invoke('run', path)
            self.assertEqual(before.returncode, 0, before.stderr)
            formatted = self.invoke('fmt', '--stdin', input=text)
            self.assertEqual(formatted.returncode, 0)
            again = self.invoke('fmt', '--stdin', input=formatted.stdout)
            self.assertEqual(again.stdout, formatted.stdout)
            path.write_text(formatted.stdout)
            after = self.invoke('run', path)
            self.assertEqual(after.returncode, 0, after.stderr)
            self.assertEqual(after.stdout, before.stdout)

    def test_repl_retains_function_bodies(self):
        result = self.invoke('repl', input='fn double(x) => x * 2\ndouble(21)\n:exit\n')
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertRegex(result.stdout, r'=>\x1b\[0m 42')

    def test_checker_reports_missing_files(self):
        result = self.invoke('check', self.work / 'missing.nv', '--format=json')
        self.assertEqual(result.returncode, 1)
        self.assertTrue(json.loads(result.stdout))
        self.assertNotIn('Check passed', result.stdout)

    def test_checker_rejects_invalid_control_flow(self):
        for text in ('return 42', 'break', 'continue', 'while true:\n    fn f():\n        break\n'):
            result = self.invoke('check', self.source(text))
            self.assertEqual(result.returncode, 1, (text, result.stdout, result.stderr))
        valid = self.invoke('check', self.source('let f = fn() { return 42 }\nprint(f())\n'))
        self.assertEqual(valid.returncode, 0, valid.stderr)

    def test_default_build_executes_and_bytecode_is_explicitly_unavailable(self):
        source = self.source('print(123)\n')
        target = self.work / 'out'
        result = self.invoke('build', source, '-o', target)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(subprocess.check_output([str(target)], text=True), '123\n')
        unavailable = self.work / 'unavailable.nvc'
        result = self.invoke('build', '--bytecode', source, '-o', unavailable)
        self.assertEqual(result.returncode, 1)
        self.assertFalse(unavailable.exists())

    def test_vm_flag_executes_vm(self):
        result = self.invoke('run', self.source('print(2 + 3)\n'), '--vm')
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(result.stdout, '5\n')

    def test_native_runtime_errors_and_nil_returns(self):
        self.assertEqual(self.native_equivalent('fn empty():\n    let x = 1\nprint(empty())\n'), 'nil\n')
        for text in ('let a = [1]\nprint(a[2])\n',
                     'fn typed(x: int) => x\nfn apply(f) => f("wrong")\nprint(apply(typed))\n'):
            source = self.source(text)
            interpreted = self.invoke('run', source)
            self.assertEqual(interpreted.returncode, 1)
            target = self.work / 'runtime-error'
            built = self.invoke('build', source, '-o', target)
            self.assertEqual(built.returncode, 0, built.stderr)
            native = subprocess.run([str(target)], capture_output=True, text=True, timeout=10)
            self.assertEqual(native.returncode, 1, native.stderr)

    def test_unsupported_native_features_fail_cleanly(self):
        for text in ('import data\nprint(data.mean([1, 2]))\n', 'print([1, 2][0:1])\n', 'for i in 0..2:\n    let f = fn() => i\n', 'let a = [1]\na[0] += 2\n'):
            target = self.work / 'unsupported'
            result = self.invoke('build', self.source(text), '-o', target)
            self.assertEqual(result.returncode, 1)
            self.assertFalse(target.exists())

    def test_http_preserves_arguments_without_shell_expansion(self):
        fakebin = self.work / 'fakebin'
        fakebin.mkdir()
        curl = fakebin / 'curl'
        curl.write_text('#!/usr/bin/python3\nimport os, sys, json\nwith open(os.environ["NV_CAPTURE_ARGS"],"w") as f: json.dump(sys.argv[1:],f)\nprint("response\\n__NV_STATUS__:201", end="")\n')
        curl.chmod(0o755)
        marker = self.work / 'injected'
        capture = self.work / 'argv.json'
        env = dict(os.environ, PATH=str(fakebin) + ':' + os.environ['PATH'], NV_CAPTURE_ARGS=str(capture))
        payload = '$(touch ' + str(marker) + ')'
        url = 'http://example.invalid/' + payload
        code = 'import http\nlet r = http.post(' + json.dumps(url) + ', ' + json.dumps(payload) + ', {"X-Test": ' + json.dumps(payload) + '})\nprint(r.status)\nprint(r.body)\n'
        result = self.invoke('eval', code, env=env)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(result.stdout, '201\nresponse\n')
        self.assertFalse(marker.exists())
        argv = json.loads(capture.read_text())
        self.assertEqual(argv[argv.index('--url') + 1], url)
        self.assertEqual(argv[argv.index('--data-raw') + 1], payload)
        self.assertEqual(argv[argv.index('--header') + 1], 'X-Test: ' + payload)
        for call in ('http.get("file:///etc/passwd")', 'http.get("http://example.invalid", {"Bad\\nHeader": "x"})',
                     'http.request("GET;touch bad", "http://example.invalid")'):
            result = self.invoke('eval', 'import http\n' + call, env=env)
            self.assertEqual(result.returncode, 1, result.stderr)


if __name__ == '__main__':
    unittest.main(verbosity=2)
