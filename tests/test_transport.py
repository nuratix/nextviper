"""Black-box regression checks for real HTTP streaming and interactive pipes."""
import json
import os
from pathlib import Path
import subprocess
import ssl
import sys
import tempfile
import unittest
from transport_fixture import Fixture

ROOT = Path(__file__).resolve().parents[1]
BIN = Path(os.environ.get('NEXTVIPER_BIN', ROOT / 'bin/nextviper')).resolve()
PEER = ROOT / 'tests/fixtures/stdio_peer.py'


class TransportTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.fixture = Fixture().__enter__()

    @classmethod
    def tearDownClass(cls):
        cls.fixture.__exit__()

    def run_nv(self, source, **kwargs):
        with tempfile.TemporaryDirectory(prefix='nv-transport-') as folder:
            path = Path(folder) / 'check.nv'
            path.write_text(source)
            result = subprocess.run([str(BIN), 'run', str(path)], capture_output=True, text=True, timeout=12, **kwargs)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            return result.stdout

    def spawn(self, mode, extra=None, options=''):
        args = [str(PEER), mode] + (extra or [])
        return 'process.spawn({"command":' + json.dumps(sys.executable) + ',"args":' + json.dumps(args) + options + '})'

    def test_incremental_delivery_headers_and_cancel(self):
        base = json.dumps(self.fixture.base)
        source = '''import std.http
let base = BASE
let seen = {"text": "", "headers": false}
let result = http.stream({"url": base + "/stream", "timeout_ms": 3500, "on_headers": fn(meta) { seen.headers = meta.headers["content-type"] == "text/event-stream"; return true }}, fn(chunk) {
    seen.text = seen.text + chunk
    if seen.text == "one" { assert(http.exchange({"url": base + "/gate"}).ok, "Release server") }
    return true
})
assert(result.ok and seen.text == "onetwo" and seen.headers, "Callback must run before server can finish")
let actual = http.exchange({"url": base + "/headers"})
assert(actual.data.headers["x-actual"] == "real-header", "Actual header values")
let count = {"n": 0}
let cancelled = http.stream({"url": base + "/cancel"}, fn(chunk) { count.n = count.n + 1; return false })
assert(cancelled.ok and cancelled.data.cancelled and count.n == 1, "Prompt cancellation")
let errors = {"count": 0}
let rejected = http.stream({"url": base + "/error"}, fn(chunk) { errors.count = errors.count + 1; return true })
assert(rejected.ok and rejected.data.status == 429 and errors.count == 0, "Do not emit error bodies as tokens")
'''.replace('BASE', base)
        self.run_nv(source)

    def test_timeout_size_validation_and_no_redirect(self):
        self.run_nv('''import std.http
let base = BASE
assert(http.exchange({"url": base + "/slow", "timeout_ms": 25}).error.kind == "timeout", "Deadline")
assert(http.exchange({"url": base + "/large", "max_response_bytes": 50}).error.kind == "limit", "Byte cap")
assert(http.exchange({"url": base + "/compressed"}).data.body == "compressed-body", "HTTP decoding")
assert(http.exchange({"url": base + "/compressed", "max_response_bytes": 3}).error.kind == "limit", "Decoded byte cap")
assert(http.exchange({"url": base + "/redirect"}).data.status == 302, "No implicit redirects")
assert(not http.exchange({"url": "file:///etc/passwd"}).ok, "HTTP schemes only")
assert(not http.exchange({"url": base, "headers": {"X-Test": "a\\nb"}}).ok, "Header injection rejected")
assert(not http.exchange({"url": base, "timeout_ms": 0}).ok, "Validate timeout")
assert(not http.stream({"url": base}, nil).ok, "Validate callback")
'''.replace('BASE', json.dumps(self.fixture.base)))
        self.assertEqual(self.fixture.redirect_hits, 0)

    def test_tls_verification_and_explicit_trust(self):
        with tempfile.TemporaryDirectory(prefix='nv-tls-') as folder:
            certificate, key = Path(folder) / 'cert.pem', Path(folder) / 'key.pem'
            subprocess.run(['openssl', 'req', '-x509', '-newkey', 'rsa:2048', '-nodes', '-days', '1',
                            '-subj', '/CN=localhost', '-addext', 'subjectAltName=IP:127.0.0.1',
                            '-keyout', str(key), '-out', str(certificate)], check=True, capture_output=True)
            peer = Fixture()
            context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
            context.load_cert_chain(certificate, key)
            peer.server.socket = context.wrap_socket(peer.server.socket, server_side=True)
            url = peer.base.replace('http://', 'https://') + '/headers'
            with peer:
                self.run_nv('import std.http\nassert(not http.exchange({"url": URL}).ok, "Self-signed certificate must fail")\n'.replace('URL', json.dumps(url)))
                env = os.environ.copy()
                env['CURL_CA_BUNDLE'] = str(certificate)
                self.run_nv('import std.http\nlet r = http.exchange({"url": URL})\nassert(r.ok and r.data.status == 200, "Explicit CA trust")\n'.replace('URL', json.dumps(url)), env=env)

    def test_subprocess_argv_environment_stderr_and_cleanup(self):
        dangerous = 'literal;$(echo not-executed) `echo x` " quote'
        setup = self.spawn('echo', [dangerous], ',"inherit_env":["NV_TEST_ALLOWED"]')
        source = '''import std.process
import std.json
let opened = SPAWN
assert(opened.ok, "Start peer")
let child = opened.data
let first = child.read_line(1500)
assert(first.ok, "Drain large stderr while reading stdout")
let data = json.parse(first.data)
assert(data.argv[0] == ARG, "Argument remains literal")
assert(data.secret == nil and data.safe == "allowed-value", "Explicit environment only")
assert(child.write("hello\\n").ok and child.read_line(500).data == "hello", "Interactive round trip")
assert(child.stderr().data.truncated, "Bounded separate stderr")
assert(child.close().ok and child.close().ok, "Idempotent cleanup")
assert(not child.write("after-close").ok, "Closed handle")
print(child.pid)
'''.replace('SPAWN', setup).replace('ARG', json.dumps(dangerous))
        env = os.environ.copy()
        env.update(NV_TEST_PRIVATE='fixture-must-not-be-inherited', NV_TEST_ALLOWED='allowed-value')
        pid = int(self.run_nv(source, env=env).strip())
        with self.assertRaises(ProcessLookupError):
            os.kill(pid, 0)

    def test_read_timeout_oversized_output_and_sigpipe(self):
        self.run_nv('''import std.process
let sleeping = SLEEP
assert(sleeping.ok, "Start sleeping peer")
assert(sleeping.data.read_line(25).error.kind == "timeout", "Bounded read")
sleeping.data.close()
let huge = HUGE
assert(huge.data.read_line(2000).error.kind == "limit", "Bounded stdout")
huge.data.close()
let ended = CLOSED
assert(ended.data.read_line(1000).data == "ready", "Peer closed stdin")
assert(not ended.data.write("test\\n").ok, "Closed pipe cannot SIGPIPE the interpreter")
ended.data.close()
assert(not process.spawn({"command":"/definitely/missing","args":[]}).ok, "Spawn failure")
assert(not process.spawn({"command":"cat","args":[1]}).ok, "Argument types")
'''.replace('SLEEP', self.spawn('sleep')).replace('HUGE', self.spawn('huge')).replace('CLOSED', self.spawn('closed')))

    @unittest.skipUnless(sys.platform.startswith('linux'), 'Linux close-from check')
    def test_unrelated_descriptors_are_not_inherited(self):
        with tempfile.NamedTemporaryFile(prefix='nv-private-descriptor-') as private:
            fd = private.fileno()
            source = 'import std.process\nlet p = ' + self.spawn('fds', [str(fd)]) + '\nassert(p.data.read_line(1000).data == "closed", "Unrelated descriptor leaked")\np.data.close()\n'
            self.run_nv(source, pass_fds=(fd,))


if __name__ == '__main__':
    unittest.main()
