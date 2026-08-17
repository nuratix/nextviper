# NextViper HTTP Implementation (`std.http` & `std.net`)

## Architecture Overview
The NextViper HTTP subsystem provides an embeddable, asynchronous, multi-threaded HTTP server and HTTP client directly integrated with NextViper user-defined closures and runtime data structures.

---

## 1. HTTP Server (`http.server()`)

### Instantiation and Route Registration
The server exposes an Express-like routing interface:
```nv
import http
import time

let app = http.server()

// Basic GET route
app.get("/", fn(req) {
    return {
        "status": 200,
        "body": "Welcome to NextViper HTTP API"
    }
})

// Dynamic route with path parameters
app.get("/users/:id", fn(req) {
    let user_id = req.params["id"]
    return {
        "status": 200,
        "body": {"id": user_id, "name": "User_" + user_id}
    }
})

// POST route with JSON body parsing
app.post("/api/data", fn(req) {
    let payload = req.json()
    return {
        "status": 201,
        "headers": {"x-server": "NextViper"},
        "body": {"status": "created", "data": payload}
    }
})
```

### Request Object (`req`)
Passed directly into route handlers with full request metadata:
- `req.method`: String HTTP method (`"GET"`, `"POST"`, `"PUT"`, `"DELETE"`, `"PATCH"`).
- `req.path`: Requested resource path.
- `req.query`: Object containing parsed URL query parameters (e.g. `?page=2&limit=50`).
- `req.params`: Object containing matched path parameter tokens (`:id`, `:slug`).
- `req.headers`: Object with normalized lowercase HTTP request headers.
- `req.body`: Raw request payload string.
- `req.json()`: Native JSON parser returning deserialized NextViper object/array structures.

### Middleware Pipeline (`app.use`)
```nv
app.use(fn(req) {
    print("[" + req.method + "] " + req.path)
})
```

### Static File Serving (`app.static`)
```nv
app.static("/public", "./static_assets")
```

### Listener & Thread Management (`app.listen` and `app.close`)
```nv
// Start server on port 8080 (background thread = true)
app.listen(8080, "0.0.0.0", true)

// Terminate socket listener and detach worker threads
app.close()
```

---

## 2. HTTP Client (`http.get`, `http.post`, `http.put`, `http.delete`, `http.request`)
Executes real outgoing HTTP requests:
```nv
let res = http.get("https://api.example.com/status")
print("Status Code:", res.status)
print("Response Text:", res.text)
let data = res.json()
```
