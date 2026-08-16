# NextViper Backend Developer Manual

Welcome to NextViper, the modern, ultra-fast programming language engineered for backend systems, distributed services, REST APIs, software engineering, and machine learning infrastructure.

---

## Table of Contents

1. [Introduction & Positioning](#1-introduction--positioning)
2. [Project Setup & Structure](#2-project-setup--structure)
3. [Building a REST API](#3-building-a-rest-api)
4. [Database Integration with PostgreSQL](#4-database-integration-with-postgresql)
5. [Authentication & Cryptography](#5-authentication--cryptography)
6. [Data Processing & AI Serving](#6-data-processing--ai-serving)
7. [Frontend Integration](#7-frontend-integration)
8. [Production Deployment & Containerization](#8-production-deployment--containerization)

---

## 1. Introduction & Positioning

**NextViper is NOT a frontend framework.** It is a standalone backend language that replaces Node.js, Python, or Go for server applications, APIs, database logic, AI model inference, and native software development. Modern frontends (React, Vue, HTML/CSS, Flutter) connect to NextViper via standard JSON REST APIs.

---

## 2. Project Setup & Structure

Initialize a new NextViper backend service:

```bash
nextviper init my_backend_service
cd my_backend_service
```

Recommended Directory Structure:
```
my_backend_service/
├── .env                  # Environment configuration
├── nextviper.toml        # Package manifest & dependencies
├── src/
│   ├── main.nv           # Application entrypoint
│   ├── routes/           # REST API route handlers
│   ├── services/         # Business domain logic
│   └── models/           # Database models & schemas
└── tests/
    └── api_test.nv       # API integration tests
```

---

## 3. Building a REST API

```nextviper
import std.http
import std.log

log.info("Starting REST API Server...")
let app = http.server()

fn get_health(req):
    return {"status": "ok", "uptime_sec": 120}

fn get_user(req):
    let user_id = req["params"]["id"]
    return {"id": user_id, "name": "Viper User", "role": "admin"}

app.get("/health", get_health)
app.get("/api/users/:id", get_user)

app.listen(8080, "0.0.0.0")
```

---

## 4. Database Integration with PostgreSQL

```nextviper
import std.db
import std.env

env.load(".env")
let db_client = db.postgres(env.require("DATABASE_URL"))

// Safe parameterized query
let res = db_client.query("SELECT id, email, role FROM users WHERE active = $1", [true])
for user in res["rows"]:
    io.print("Active user: " + user["email"])
```

---

## 5. Authentication & Cryptography

```nextviper
import std.crypto

// Password Hashing
let hash = crypto.hash_password("UserPassword123")
let is_valid = crypto.verify_password("UserPassword123", hash)

// JWT Token Issuance
let token = crypto.jwt_encode({"user_id": 42, "role": "admin"}, "jwt_secret_key")
let claims = crypto.jwt_decode(token, "jwt_secret_key")
```

---

## 6. Frontend Integration

Frontends communicate with NextViper over HTTP:

```javascript
// React / Next.js / Vue client
const response = await fetch("http://localhost:8080/api/users/42");
const userData = await response.json();
console.log(userData.name);
```
