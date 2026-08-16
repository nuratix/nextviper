# NextViper Frontend Integration Guide

NextViper is a dedicated **backend, systems, and software engineering language**. It is not a frontend framework or HTML template engine. Modern frontends (React, Vue, Next.js, Angular, Svelte, Mobile Apps) connect to NextViper seamlessly via standardized JSON REST APIs.

---

## 1. Architecture Flow

```
+------------------------------------+           +------------------------------------+
|          Frontend Client           |           |         NextViper Backend          |
|                                    |           |                                    |
|   - React (Next.js, Vite, CRA)     |           |   - HTTP Server (std.http)         |
|   - Vue / Nuxt                     |   JSON    |   - PostgreSQL Database (std.db)   |
|   - Mobile (React Native, Flutter) | <=======> |   - Authentication (std.crypto)    |
|   - Native Desktop Apps            |   REST    |   - Machine Learning (std.ai)      |
|                                    |           |   - Columnar Analytics (std.data)  |
+------------------------------------+           +------------------------------------+
```

---

## 2. NextViper Backend API (`server.nv`)

```nextviper
import std.http
import std.log

let app = http.server()

app.get("/api/status", fn(req):
    return {
        "status": "online",
        "service": "NextViper Production API",
        "version": "1.0.0"
    }
)

app.post("/api/calculate", fn(req):
    let body = req.json()
    let a = body["a"]
    let b = body["b"]
    return {
        "operation": "add",
        "result": a + b
    }
)

app.listen(8080, "0.0.0.0")
```

---

## 3. React Frontend Integration (`App.jsx` / `App.tsx`)

```typescript
import React, { useEffect, useState } from 'react';

export function NextViperClient() {
  const [status, setStatus] = useState(null);
  const [result, setResult] = useState(null);

  useEffect(() => {
    // 1. Query NextViper Backend API
    fetch('http://localhost:8080/api/status')
      .then(res => res.json())
      .then(data => setStatus(data));
  }, []);

  const handleCompute = async () => {
    // 2. Post calculation to NextViper Backend
    const res = await fetch('http://localhost:8080/api/calculate', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ a: 15, b: 27 })
    });
    const data = await res.json();
    setResult(data.result);
  };

  return (
    <div className="card">
      <h2>NextViper Backend Connected</h2>
      <p>Service: {status?.service || 'Connecting...'}</p>
      <button onClick={handleCompute}>Compute 15 + 27</button>
      {result !== null && <p>Result from NextViper: {result}</p>}
    </div>
  );
}
```
