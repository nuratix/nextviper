#!/usr/bin/env python3
"""Exercise the shipped application against a real, dedicated PostgreSQL DB."""
import http.client
from contextlib import closing
import json
import os
from pathlib import Path
import subprocess
import tempfile
import time
import unittest
from test_http_hardening import available_port

ROOT = Path(__file__).resolve().parents[1]
BIN = Path(os.environ.get('NEXTVIPER_BIN', ROOT / 'bin/nextviper')).resolve()
DATABASE_URL = os.environ.get('NEXTVIPER_TEST_DATABASE_URL')
TOKEN = 'nextviper-local-integration-test-token'


@unittest.skipUnless(DATABASE_URL, 'set NEXTVIPER_TEST_DATABASE_URL to a dedicated test database')
class BackendContract(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='nv-backend-')
        self.addCleanup(self.temp.cleanup)
        self.work = Path(self.temp.name)
        self.logs = (self.work / 'server.log').open('w+')
        self.addCleanup(self.logs.close)
        self.process = None
        self.created = []
        self.addCleanup(self.cleanup)
        self.start()

    def start(self):
        self.port = available_port()
        environment = dict(os.environ, DATABASE_URL=DATABASE_URL, API_TOKEN=TOKEN,
                           PORT=str(self.port), HOST='127.0.0.1')
        self.process = subprocess.Popen([str(BIN), 'run', str(ROOT / 'examples/todo-api/app.nv')],
                                        cwd=self.work, env=environment, stdout=self.logs, stderr=self.logs)
        deadline = time.monotonic() + 20
        while time.monotonic() < deadline:
            if self.process.poll() is not None:
                self.logs.seek(0)
                self.fail('backend exited: ' + self.logs.read())
            try:
                if self.request('GET', '/health', authenticated=False)[0] == 200:
                    return
            except OSError:
                time.sleep(0.05)
        self.fail('backend did not become healthy')

    def stop(self):
        if self.process is not None and self.process.poll() is None:
            self.process.terminate()
            try:
                self.process.wait(timeout=10)
            except subprocess.TimeoutExpired:
                self.process.kill()
                self.process.wait()

    def cleanup(self):
        if self.process is not None and self.process.poll() is None:
            for identifier in self.created:
                self.request('DELETE', f'/tasks/{identifier}')
        self.stop()

    def request(self, method, path, body=None, authenticated=True):
        headers = {'Content-Type': 'application/json'}
        if authenticated:
            headers['Authorization'] = 'Bearer ' + TOKEN
        if body is not None and not isinstance(body, str):
            body = json.dumps(body)
        with closing(http.client.HTTPConnection('127.0.0.1', self.port, timeout=10)) as connection:
            connection.request(method, path, body, headers)
            response = connection.getresponse()
            payload = response.read()
            return response.status, json.loads(payload) if payload else None

    def test_crud_unicode_sql_parameters_and_restart_persistence(self):
        title = "Learn é🚀; '); DROP TABLE nextviper_tasks; --"
        status, task = self.request('POST', '/tasks', {'title': title})
        self.assertEqual(status, 201, task)
        identifier = task['id']
        self.created.append(identifier)
        self.assertEqual(task, {'id': identifier, 'title': title, 'done': False})
        self.assertEqual(self.request('GET', f'/tasks/{identifier}'), (200, task))
        status, listed = self.request('GET', '/tasks')
        self.assertEqual(status, 200)
        self.assertIn(task, listed['tasks'])
        self.stop()
        self.start()
        self.assertEqual(self.request('GET', f'/tasks/{identifier}'), (200, task))
        status, changed = self.request('PUT', f'/tasks/{identifier}', {'title': 'Completed', 'done': True})
        self.assertEqual(status, 200, changed)
        self.assertTrue(changed['done'])
        self.assertEqual(self.request('DELETE', f'/tasks/{identifier}'), (204, None))
        self.assertEqual(self.request('GET', f'/tasks/{identifier}')[0], 404)

    def test_authentication_and_validation(self):
        self.assertEqual(self.request('GET', '/tasks', authenticated=False)[0], 401)
        self.assertEqual(self.request('POST', '/tasks', {'title': 'x'}, authenticated=False)[0], 401)
        for body in ({}, {'title': 1}, {'title': ''}, {'title': '   '}, {'title': 'x' * 201}, [], '{'):
            self.assertEqual(self.request('POST', '/tasks', body)[0], 400, body)
        self.assertEqual(self.request('GET', '/tasks/not-an-id')[0], 400)
        self.assertEqual(self.request('GET', '/missing')[0], 404)
        self.assertEqual(self.request('GET', '/health', authenticated=False), (200, {'ok': True}))

    def test_transaction_on_closed_connection_fails_cleanly(self):
        source = self.work / 'transaction.nv'
        source.write_text('import db\nimport env\nlet client = db.postgres(env.require("DATABASE_URL"))\n'
                          'client.close()\nclient.transaction(fn(tx) => true)\n')
        result = subprocess.run([str(BIN), 'run', str(source)], capture_output=True, text=True,
                                timeout=20, env=dict(os.environ, DATABASE_URL=DATABASE_URL))
        self.assertEqual(result.returncode, 1, result.stderr)
        self.assertIn('closed database connection', result.stderr)


if __name__ == '__main__':
    unittest.main(verbosity=2)
