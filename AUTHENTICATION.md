# NextViper Authentication & Authorization Guide

This guide describes how to implement end-to-end user authentication, JWT session management, password hashing, and role-based access control (RBAC) in NextViper.

---

## 1. User Registration Flow

```nextviper
import std.http
import std.crypto
import std.db
import std.log

let app = http.server()
let db_conn = db.postgres("postgres://localhost:5432/auth_db")

fn handle_register(req):
    let body = req.json()
    let email = body["email"]
    let password = body["password"]

    // 1. Hash password with PBKDF2
    let pwd_hash = crypto.hash_password(password)

    // 2. Insert into database using parameterized query
    db_conn.execute(
        "INSERT INTO users (email, password_hash, role) VALUES ($1, $2, $3)",
        [email, pwd_hash, "member"]
    )

    return {"status": 201, "body": {"message": "User registered successfully"}}

app.post("/api/auth/register", handle_register)
```

---

## 2. User Login Flow & Token Issuance

```nextviper
let JWT_SECRET = "secure_app_secret_key"

fn handle_login(req):
    let body = req.json()
    let email = body["email"]
    let password = body["password"]

    // 1. Fetch user by email
    let res = db_conn.query("SELECT id, password_hash, role FROM users WHERE email = $1", [email])
    if len(res["rows"]) == 0:
        return {"status": 401, "body": {"error": "Invalid email or password"}}

    let user = res["rows"][0]

    // 2. Verify password hash
    let valid = crypto.verify_password(password, user["password_hash"])
    if !valid:
        return {"status": 401, "body": {"error": "Invalid email or password"}}

    // 3. Issue JWT Token
    let token = crypto.jwt_encode({"user_id": user["id"], "role": user["role"]}, JWT_SECRET)
    return {
        "status": 200,
        "body": {
            "token": token,
            "user": {"id": user["id"], "email": email, "role": user["role"]}
        }
    }

app.post("/api/auth/login", handle_login)
```

---

## 3. Protected Route Verification

```nextviper
fn handle_profile(req):
    let auth_header = req["headers"]["authorization"]
    if auth_header == nil:
        return {"status": 401, "body": {"error": "Authorization header missing"}}

    // Extract Bearer token
    let token = auth_header
    let user_claims = crypto.jwt_decode(token, JWT_SECRET)
    if user_claims == nil:
        return {"status": 403, "body": {"error": "Invalid or expired token"}}

    return {"status": 200, "body": {"profile": user_claims}}

app.get("/api/user/profile", handle_profile)
```
