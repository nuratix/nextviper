"""Local HTTP protocol peer for transport tests; it is not a paid AI provider."""
import contextlib
import gzip
import json
import threading
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer


class Fixture:
    def __init__(self):
        self.gate = threading.Event()
        self.roots = threading.Event()
        self.redirect_hits = 0
        self.header_checks = 0
        fixture = self

        class Handler(BaseHTTPRequestHandler):
            protocol_version = 'HTTP/1.1'

            def log_message(self, *args):
                pass

            def fixed(self, value, status=200, headers=None):
                data = json.dumps(value, ensure_ascii=False).encode() if not isinstance(value, bytes) else value
                self.send_response(status)
                self.send_header('Content-Type', 'application/json')
                for key, val in (headers or {}).items():
                    self.send_header(key, val)
                self.send_header('Content-Length', str(len(data)))
                self.end_headers()
                with contextlib.suppress(BrokenPipeError, ConnectionResetError):
                    self.wfile.write(data)
                    self.wfile.flush()

            def begin(self):
                self.send_response(200)
                self.send_header('Content-Type', 'text/event-stream')
                self.send_header('Transfer-Encoding', 'chunked')
                self.end_headers()

            def chunk(self, data):
                self.wfile.write(('%x\r\n' % len(data)).encode() + data + b'\r\n')
                self.wfile.flush()

            def event(self, value):
                self.chunk(('data: ' + json.dumps(value, ensure_ascii=False) + '\n\n').encode())

            def end(self):
                self.wfile.write(b'0\r\n\r\n')
                self.wfile.flush()

            def do_GET(self):
                try:
                    if self.path == '/mcp':
                        if self.headers.get('Mcp-Session-Id') != 'wire-session':
                            return self.fixed({}, 400)
                        self.begin()
                        self.event({'jsonrpc': '2.0', 'method': 'notifications/resources/updated', 'params': {'uri': 'test://resource'}})
                        time.sleep(.6)
                        return self.end()
                    if self.path == '/gate':
                        fixture.gate.set()
                        return self.fixed({'released': True})
                    if self.path == '/headers':
                        return self.fixed({'ok': True}, headers={'X-Actual': 'real-header'})
                    if self.path == '/redirect':
                        return self.fixed({}, 302, {'Location': '/redirect-target'})
                    if self.path == '/redirect-target':
                        fixture.redirect_hits += 1
                        return self.fixed({})
                    if self.path == '/compressed':
                        return self.fixed(gzip.compress(b'compressed-body'), headers={'Content-Encoding': 'gzip'})
                    if self.path == '/large':
                        return self.fixed(b'x' * 10000)
                    if self.path == '/slow':
                        time.sleep(.4)
                        return self.fixed({})
                    if self.path == '/error':
                        return self.fixed({'error': 'not a token stream'}, 429)
                    if self.path == '/stream':
                        fixture.gate.clear()
                        self.begin()
                        self.chunk(b'one')
                        if not fixture.gate.wait(2):
                            self.close_connection = True
                            return
                        self.chunk(b'two')
                        return self.end()
                    if self.path == '/cancel':
                        self.begin()
                        for _ in range(300):
                            self.chunk(b'piece')
                            time.sleep(.005)
                        return self.end()
                    return self.fixed({}, 404)
                except (BrokenPipeError, ConnectionResetError):
                    self.close_connection = True

            def do_POST(self):
                try:
                    size = int(self.headers.get('Content-Length', '0'))
                    if size > 100000:
                        return self.fixed({}, 413)
                    body = json.loads(self.rfile.read(size))
                    if self.path == '/mcp':
                        return self.mcp(body)
                    if self.path in ['/v1/responses', '/v1/messages', '/v1/chat/completions']:
                        return self.provider(body)
                    return self.fixed({}, 404)
                except (BrokenPipeError, ConnectionResetError):
                    self.close_connection = True

            def provider(self, body):
                if body.get('model') != 'fixture':
                    return self.fixed({'error': 'Unknown fixture model'}, 400)
                if not body.get('stream'):
                    if self.path == '/v1/responses':
                        return self.fixed({'status': 'completed', 'output': [{'type': 'message', 'content': [{'type': 'output_text', 'text': 'Hello é 😀'}]}]})
                    if self.path == '/v1/messages':
                        return self.fixed({'content': [{'type': 'text', 'text': 'Hello é 😀'}], 'stop_reason': 'end_turn'})
                    return self.fixed({'choices': [{'message': {'content': 'Hello é 😀'}, 'finish_reason': 'stop'}]})
                fixture.gate.clear()
                self.begin()
                if self.path == '/v1/responses':
                    self.event({'type': 'response.output_text.delta', 'delta': 'Hello é 😀'})
                elif self.path == '/v1/messages':
                    self.event({'type': 'message_start', 'message': {'id': 'msg1', 'usage': {'input_tokens': 1}}})
                    self.event({'type': 'content_block_delta', 'index': 0, 'delta': {'type': 'text_delta', 'text': 'Hello é 😀'}})
                else:
                    self.event({'choices': [{'index': 0, 'delta': {'content': 'Hello é 😀'}}]})
                if not fixture.gate.wait(2):
                    self.close_connection = True
                    return
                if self.path == '/v1/responses':
                    self.event({'type': 'response.completed', 'response': {'status': 'completed', 'usage': {'output_tokens': 2}}})
                elif self.path == '/v1/messages':
                    self.event({'type': 'message_delta', 'delta': {'stop_reason': 'end_turn'}, 'usage': {'output_tokens': 2}})
                    self.event({'type': 'message_stop'})
                else:
                    self.event({'choices': [{'index': 0, 'delta': {}, 'finish_reason': 'stop'}], 'usage': {'total_tokens': 3}})
                    self.chunk(b'data: [DONE]\n\n')
                self.end()

            def mcp(self, body):
                mid, method = body.get('id'), body.get('method')
                if method == 'initialize':
                    return self.fixed({'jsonrpc': '2.0', 'id': mid, 'result': {'protocolVersion': '2025-11-25', 'capabilities': {'tools': {}}, 'serverInfo': {'name': 'wire-fixture', 'version': '1'}}}, headers={'Mcp-Session-Id': 'wire-session'})
                if self.headers.get('Mcp-Session-Id') != 'wire-session' or self.headers.get('MCP-Protocol-Version') != '2025-11-25':
                    return self.fixed({}, 400)
                fixture.header_checks += 1
                if method is None and mid == 'server-roots':
                    if body.get('result', {}).get('roots', [{}])[0].get('uri') == 'file:///workspace':
                        fixture.roots.set()
                    return self.fixed(b'', 202)
                if mid is None:
                    return self.fixed(b'', 202)
                if method == 'tools/list':
                    fixture.roots.clear()
                    self.begin()
                    self.event({'jsonrpc': '2.0', 'id': 'server-roots', 'method': 'roots/list', 'params': {}})
                    if not fixture.roots.wait(1):
                        self.close_connection = True
                        return
                    self.event({'jsonrpc': '2.0', 'id': mid, 'result': {'tools': [{'name': 'add', 'inputSchema': {'type': 'object'}}]}})
                    # Deliberately leave this POST stream open after its response.
                    # A correct client stops immediately rather than waiting for EOF.
                    time.sleep(.6)
                    self.chunk(b':still open\n\n')
                    return self.end()
                result = {}
                if method == 'tools/call':
                    result = {'content': [{'type': 'text', 'text': str(sum(body['params'].get('arguments', {}).values()))}]}
                return self.fixed({'jsonrpc': '2.0', 'id': mid, 'result': result})

            def do_DELETE(self):
                if self.path == '/mcp' and self.headers.get('Mcp-Session-Id') == 'wire-session':
                    return self.fixed(b'', 204)
                return self.fixed({}, 404)

        self.server = ThreadingHTTPServer(('127.0.0.1', 0), Handler)
        self.server.daemon_threads = True
        self.base = 'http://127.0.0.1:%s' % self.server.server_port
        self.thread = threading.Thread(target=self.server.serve_forever, daemon=True)

    def __enter__(self):
        self.thread.start()
        return self

    def __exit__(self, *args):
        self.server.shutdown()
        self.server.server_close()
        self.thread.join(2)


if __name__ == '__main__':
    with Fixture() as fixture:
        print(fixture.base, flush=True)
        try:
            while True:
                time.sleep(1)
        except KeyboardInterrupt:
            pass
