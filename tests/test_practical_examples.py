#!/usr/bin/env python3
"""Run the documented applications against the actual interpreter."""
import http.client
import json
import os
from pathlib import Path
import socket
import subprocess
import time
import unittest

ROOT = Path(__file__).resolve().parents[1]
BIN = Path(os.environ.get('NEXTVIPER_BIN', ROOT / 'bin/nextviper')).resolve()
EXAMPLES = ROOT / 'examples/practical'


class PracticalExamples(unittest.TestCase):
    def run_example(self, name, data):
        return subprocess.run([str(BIN), 'run', str(EXAMPLES / name)], input=data,
                              capture_output=True, text=True, timeout=10)

    def test_calculator(self):
        for data, expected in [('+\n20\n22\n', '42\n'), ('-\n3\n5\n', '-2\n'),
                               ('*\n6\n7\n', '42\n'), ('/\n5\n2\n', '2.5\n')]:
            with self.subTest(data=data):
                result = self.run_example('calculator.nv', data)
                self.assertEqual((result.returncode, result.stdout, result.stderr), (0, expected, ''))

    def test_calculator_errors(self):
        for data, message in [('/\n1\n0\n', 'division by zero'), ('?\n1\n2\n', 'operator must')]:
            result = self.run_example('calculator.nv', data)
            self.assertEqual(result.returncode, 1)
            self.assertIn(message, result.stderr)

    def test_text_summary(self):
        for data, expected in [('hello\n\nworld\n', 'bytes: 13\nnonempty lines: 2\n'),
                               ('', 'bytes: 0\nnonempty lines: 0\n')]:
            result = self.run_example('text_summary.nv', data)
            self.assertEqual((result.returncode, result.stdout, result.stderr), (0, expected, ''))

    def test_http_health(self):
        with socket.socket() as reservation:
            reservation.bind(('127.0.0.1', 0))
            port = reservation.getsockname()[1]
        process = subprocess.Popen([str(BIN), 'run', str(EXAMPLES / 'health_server.nv')],
                                   env={**os.environ, 'PORT': str(port)},
                                   stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        try:
            deadline = time.monotonic() + 10
            while True:
                if process.poll() is not None:
                    self.fail('server exited: ' + process.communicate()[1])
                connection = http.client.HTTPConnection('127.0.0.1', port, timeout=1)
                try:
                    connection.request('GET', '/health')
                    response = connection.getresponse()
                    self.assertEqual(response.status, 200)
                    self.assertEqual(json.loads(response.read()), {'ok': True})
                    break
                except (ConnectionRefusedError, TimeoutError):
                    if time.monotonic() >= deadline:
                        self.fail('health server did not start in 10 seconds')
                    time.sleep(0.05)
                finally:
                    connection.close()
        finally:
            process.terminate()
            try:
                process.communicate(timeout=5)
            except subprocess.TimeoutExpired:
                process.kill()
                process.communicate()


if __name__ == '__main__':
    unittest.main(verbosity=2)
