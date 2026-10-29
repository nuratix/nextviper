#!/usr/bin/env python3
"""HTTP regression tests using real sockets and isolated fixtures."""
import http.client
import json
import os
from pathlib import Path
import socket
import struct
import subprocess
import tempfile
import time
import unittest

ROOT = Path(__file__).resolve().parents[1]
BIN = Path(os.environ.get('NEXTVIPER_BIN', ROOT / 'bin/nextviper')).resolve()


def available_port():
    with socket.socket() as connection:
        connection.bind(('127.0.0.1', 0))
        return connection.getsockname()[1]


class HttpServerContract(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temp = tempfile.TemporaryDirectory(prefix='nv-http-')
        cls.work = Path(cls.temp.name)
        cls.port = available_port()
        public = cls.work / 'public'
        public.mkdir()
        (public / 'index.html').write_text('public index')
        (cls.work / 'secret.txt').write_text('PRIVATE_FILE_MARKER')
        script = cls.work / 'server.nv'
        script.write_text('import http\nlet app = http.server()\n'
                          'app.static("/static", ' + json.dumps(str(public)) + ')\n'
                          'app.get("/health", fn(req) => {"ok": true})\n'
                          'app.get("/query", fn(req) => req.query)\n'
                          'app.post("/echo", fn(req) => {"status": 201, "body": req.json()})\n'
                          'app.get("/error", fn(req) { assert(false, "PRIVATE_ERROR_MARKER") })\n'
                          'app.get("/header", fn(req) => {"status": 200, "body": "x", "headers": {"X-Test": "a\\r\\nInjected: yes"}})\n'
                          f'app.listen({cls.port}, "127.0.0.1")\n')
        cls.logs = (cls.work / 'server.log').open('w+')
        cls.process = subprocess.Popen([str(BIN), 'run', str(script)], stdout=cls.logs, stderr=cls.logs)
        cls.addClassCleanup(cls.cleanup)
        deadline = time.monotonic() + 20
        while time.monotonic() < deadline:
            if cls.process.poll() is not None:
                cls.logs.seek(0)
                raise RuntimeError('HTTP fixture exited: ' + cls.logs.read())
            try:
                with socket.create_connection(('127.0.0.1', cls.port), timeout=0.2):
                    return
            except OSError:
                time.sleep(0.05)
        raise RuntimeError('HTTP fixture did not start')

    @classmethod
    def cleanup(cls):
        if cls.process.poll() is None:
            cls.process.terminate()
            try:
                cls.process.wait(timeout=10)
            except subprocess.TimeoutExpired:
                cls.process.kill()
                cls.process.wait()
        cls.logs.close()
        cls.temp.cleanup()

    def request(self, method, path, body=None):
        connection = http.client.HTTPConnection('127.0.0.1', self.port, timeout=10)
        self.addCleanup(connection.close)
        connection.request(method, path, body=body)
        response = connection.getresponse()
        return response.status, response.read()

    def raw(self, payload, fragments=None):
        with socket.create_connection(('127.0.0.1', self.port), timeout=10) as connection:
            for fragment in fragments or [payload]:
                connection.sendall(fragment)
                if fragments:
                    time.sleep(0.02)
            connection.shutdown(socket.SHUT_WR)
            result = b''
            while True:
                try:
                    chunk = connection.recv(65536)
                except ConnectionResetError:
                    break
                if not chunk:
                    break
                result += chunk
            return result

    def test_static_files_and_traversal(self):
        self.assertEqual(self.request('GET', '/static/index.html'), (200, b'public index'))
        for path in ('/static/../secret.txt', '/static/%2e%2e/secret.txt'):
            status, body = self.request('GET', path)
            self.assertEqual(status, 404)
            self.assertNotIn(b'PRIVATE_FILE_MARKER', body)

    def test_query_and_unicode_json(self):
        status, body = self.request('GET', '/query?search=Hello%20World%21')
        self.assertEqual(status, 200)
        self.assertEqual(json.loads(body), {'search': 'Hello World!'})
        status, body = self.request('POST', '/echo', b'{"text":"\\u00e9\\uD83D\\uDE80"}')
        self.assertEqual(status, 201)
        self.assertEqual(json.loads(body), {'text': 'é🚀'})

    def test_fragmented_request(self):
        body = b'{"msg":"ok"}'
        response = self.raw(b'', fragments=[b'POST /echo HTTP/1.1\r\nHost: local\r\n',
                                            f'Content-Length: {len(body)}\r\n\r\n'.encode(), body[:5], body[5:]])
        self.assertIn(b'201 Created', response)
        self.assertEqual(json.loads(response.split(b'\r\n\r\n', 1)[1]), {'msg': 'ok'})

    def test_invalid_json_is_400(self):
        for body in (b'{', b'[', b'{} garbage', b'{"x":1', b'[1,]', b'01', b'1.', b'1e+',
                     b'"\\q"', b'"\\uD800"', b'"\\uDC00"', b'"line\nfeed"', b'"\xff"', b'"\xc0\x80"',
                     b'"\xed\xa0\x80"', b'[' * 150 + b']' * 150):
            with self.subTest(body=body[:40]):
                status, payload = self.request('POST', '/echo', body)
                self.assertEqual(status, 400)
                self.assertEqual(json.loads(payload), {'error': 'Bad Request'})

    def test_malformed_http_framing(self):
        for headers, status in ((b'Content-Length: abc\r\n', 400), (b'Content-Length: -1\r\n', 400),
                                (b'Content-Length: 0\r\nContent-Length: 1\r\n', 400),
                                (b'Transfer-Encoding: chunked\r\n', 501), (b'Content-Length: 10485761\r\n', 413),
                                (b'Content-Length: 10\r\n', 400), (b'Expect: 100-continue\r\n', 417)):
            with self.subTest(headers=headers):
                response = self.raw(b'POST /echo HTTP/1.1\r\nHost: local\r\n' + headers + b'\r\n')
                self.assertTrue(response.startswith(f'HTTP/1.1 {status} '.encode()), response)
        response = self.raw(b'GET /health HTTP/2.0\r\nHost: local\r\n\r\n')
        self.assertTrue(response.startswith(b'HTTP/1.1 400 '), response)

    def test_oversized_header(self):
        response = self.raw(b'GET /health HTTP/1.1\r\nHost: local\r\nX-Huge: ' + b'a' * 18000 + b'\r\n\r\n')
        self.assertTrue(response.startswith(b'HTTP/1.1 431 '), response)

    def test_handler_errors_do_not_leak_or_split_responses(self):
        for path in ('/error', '/header'):
            status, body = self.request('GET', path)
            self.assertEqual(status, 500)
            self.assertEqual(json.loads(body), {'error': 'Internal Server Error'})
        status, body = self.request('GET', '/missing%22path')
        self.assertEqual(status, 404)
        self.assertEqual(json.loads(body)['path'], '/missing"path')

    def test_disconnected_client_does_not_kill_server(self):
        for _ in range(5):
            with socket.create_connection(('127.0.0.1', self.port), timeout=5) as connection:
                connection.sendall(b'GET /health HTTP/1.1\r\nHost: local\r\n\r\n')
                connection.setsockopt(socket.SOL_SOCKET, socket.SO_LINGER, struct.pack('ii', 1, 0))
        self.assertEqual(self.request('GET', '/health'), (200, b'{"ok":true}'))

    def test_background_server_can_close_cleanly(self):
        script = self.work / 'close.nv'
        script.write_text('import http\nlet app = http.server()\n'
                          f'app.listen({available_port()}, "127.0.0.1", true)\napp.close()\nprint("closed")\n')
        result = subprocess.run([str(BIN), 'run', str(script)], capture_output=True, text=True, timeout=10)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(result.stdout, 'closed\n')


if __name__ == '__main__':
    unittest.main(verbosity=2)
