# NextViper HTTP Server & REST API Framework

The `std.http` module provides native HTTP server and client capabilities with automatic JSON routing and parameter parsing.

---

## 1. HTTP Server API

### Server Initialization
```nextviper
import std.http

let app = http.server()
```

### Route Registration
```nextviper
// HTTP Methods
app.get(path_pattern, handler_fn)
app.post(path_pattern, handler_fn)
app.put(path_pattern, handler_fn)
app.delete(path_pattern, handler_fn)
app.patch(path_pattern, handler_fn)

// Static file serving
app.static(url_prefix, directory_path)

// Middleware registration
app.use(middleware_fn)

// Start Server Loop
app.listen(port, host, background)
```

---

## 2. Request Object Structure

When a route handler is invoked, it receives a `req` map with:

- `req["method"]` (String): e.g. `"GET"`, `"POST"`
- `req["path"]` (String): e.g. `"/api/users/42"`
- `req["params"]` (Map): Extracted path parameters, e.g. `{"id": "42"}`
- `req["query"]` (Map): Parsed query string, e.g. `{"page": "2", "limit": "20"}`
- `req["headers"]` (Map): Lowercased request headers, e.g. `{"authorization": "Bearer ..."}`
- `req["body"]` (String): Raw request payload
- `req.json()` (Function): Deserializes body to NextViper Value objects

---

## 3. Response Handling

Handlers can return:
1. **Objects / Maps**: Serialized to JSON with status 200 and `Content-Type: application/json`.
2. **Strings**: Returned as text/html with status 200.
3. **Structured Response Objects**:
   ```nextviper
   return {
       "status": 201,
       "body": {"id": 101, "name": "Item"},
       "headers": {"X-Custom-Header": "Value"}
   }
   ```

## Incremental transport APIs

`http.exchange`, `http.stream` and interactive `process.spawn` are described with their limits and result interfaces in [TRANSPORT_API.md](TRANSPORT_API.md). They support real streaming and MCP clients on the interpreter.

## Server limits and validation

The server handles one request at a time and closes each connection after the
response. Request headers are limited to 16 KiB and bodies to 10 MiB, with socket
read/write timeouts. HTTP/1.0 and HTTP/1.1 origin-form requests are supported.
Duplicate/invalid Content-Length is rejected. Chunked request bodies return 501;
Expect requests return 417. There is no HTTP/2, TLS, or WebSocket server support.

Malformed `req.json()` input returns 400; JSON nesting is limited to 128 levels.
Unicode escapes and surrogate pairs are decoded to UTF-8. Handler failures return
a generic 500 without internal SQL/error messages. Response headers are validated
and framing headers are managed by the server. Register routes before listening.
`app.close()` shuts down the listener and joins a background server thread.
Use foreground listening for ordinary backend apps.

The [complete task API](examples/todo-api/README.md) includes validation,
authentication, parameterized SQL, and a deployment guide. Authentication and
application authorization remain the application's responsibility.
