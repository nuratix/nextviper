# Incremental HTTP and interactive processes

These interpreter APIs support streaming AI clients and MCP transports. The SDK built on them can remain `.nv`; networking and process primitives live in the existing C++ runtime. The additions require libcurl 7.85+; the interactive process implementation is POSIX and is tested on Linux ARM64. VM/native-AOT support is not established.

## HTTP

`http.exchange(config)` returns a result object with `ok`, `data`, `error`. Config:

- `url`: explicit HTTP(S) URL without embedded user credentials or control bytes.
- `method`: uppercase method, default GET; `headers`: string map; `body`: encoded string.
- `timeout_ms`: 1–300000, default 30000. Connection timeout is at most ten seconds.
- `max_response_bytes`: 1–16777216, default 16777216; request body maximum is 8 MiB.
- Optional `on_headers(metadata)` callback. Return false to stop.

Response data contains `status`, actual lowercase `headers`, string `body` and `cancelled`. HTTP error statuses are returned as data; invalid config, timeout, network and limit failures return a failed result. Compressed responses are decoded by libcurl and the body byte cap applies to decoded bytes. Response headers are capped at 64 KiB / 128 names. Duplicate header names retain the last value; this API is not a cookie jar.

`http.stream(config, on_chunk)` invokes the callback with raw body-byte strings incrementally, on the calling interpreter thread. A callback is not one JSON frame or necessarily a complete Unicode character. The optional header callback runs first. Returning false cancels promptly; the response has `cancelled: true`. Only 2xx bodies reach the stream callback; error bodies are not emitted as model text. Streaming does not accumulate a second copy of the body. Callback exceptions unwind after libcurl cleanup rather than crossing a C callback boundary.

HTTPS certificate verification is always enabled. Normal libcurl proxy behavior is retained; `CURL_CA_BUNDLE` or `SSL_CERT_FILE` can select an explicit trust file. No automatic redirects, netrc credentials, cookie persistence or retries are enabled. Request keys/bodies are not put into process arguments. Errors avoid dumping URLs, request bodies or authentication headers. The API does not provide a general application SSRF policy; choose trusted endpoints and credentials.

Legacy `http.get/post/request` retain their previous curl-executable behavior. Use the new result APIs for recoverable transport errors and actual headers.

## Subprocesses

`process.spawn({command, args, env, inherit_env, max_line_bytes})` returns a result containing a handle. It uses an executable and argv; it never interpolates a shell command. `cwd` is explicitly unsupported. PATH defaults to `/usr/bin:/bin`, LANG to `C.UTF-8`; no other environment variables are inherited automatically. Use `env` or `inherit_env` explicitly for required settings. Maximum 256 arguments / 128 KiB total argument and environment bytes.

Handle methods:

- `write(text, timeout_ms)` returns bytes written; timeout defaults to 30000 ms. Maximum write 1 MiB.
- `read_line(timeout_ms)` returns a line string, nil on EOF, or a failed result. `max_line_bytes` bounds the stdout buffer, 1024–1048576, default 1048576.
- `stderr()` returns `{text, truncated}` separately from stdout. Retains at most 64 KiB; total diagnostic traffic above 16 MiB aborts the handle.
- `close()` closes pipes, terminates the process group and reaps the child, returning its exit code. Repeated close is safe. Releasing the last handle also cleans up, subject to the interpreter's documented reference-cycle limitations.
- `pid` identifies the spawned child for diagnostics.

Reads/writes drain pipes without mixing stderr into JSON-RPC and enforce steady-clock deadlines. Pipe writes do not globally change SIGPIPE handling. Pipe descriptors are close-on-exec; on glibc unrelated descriptors are closed in the spawned child. This is not an executable or filesystem sandbox. Use trusted commands and OS isolation for untrusted servers.

`time.monotonic_ms()` supports request deadlines without wall-clock adjustments.

## Validation

`python3 tests/test_transport.py` covers actual incremental delivery (a callback must release the server before it can complete), actual response headers, cancellation, status-error suppression, timeout/size/scheme/header checks, disabled redirects, HTTPS trust, interactive argv/env handling, separated large stderr, EOF/limits, SIGPIPE resilience, descriptor isolation and child reaping. Fixtures are local protocol peers, not AI models.

Reference behavior: [libcurl write callbacks](https://curl.se/libcurl/c/CURLOPT_WRITEFUNCTION.html), [header callbacks](https://curl.se/libcurl/c/CURLOPT_HEADERFUNCTION.html), [timeouts](https://curl.se/libcurl/c/CURLOPT_TIMEOUT_MS.html), and [MCP transports](https://modelcontextprotocol.io/specification/2025-11-25/basic/transports).
