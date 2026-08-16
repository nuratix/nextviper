# NextViper Backend Security Specification

Security in NextViper backend systems is built into the language primitives and standard library, ensuring zero plaintext secrets, safe parameter binding, and robust cryptography.

---

## 1. Cryptographic Password Hashing

NextViper uses **PBKDF2-SHA256** key derivation with cryptographic salt generation (10,000 iterations default) to store and verify passwords:

```nextviper
import std.crypto

// Hash password with unique salt
let password_hash = crypto.hash_password("SuperSecretUserPassword!")

// Verify password against stored hash (constant-time evaluation)
let is_authenticated = crypto.verify_password("SuperSecretUserPassword!", password_hash)
```

Stored hash format: `pbkdf2_sha256$<iterations>$<salt>$<digest>`

---

## 2. JSON Web Token (JWT) Authentication

NextViper implements RFC 7519 JWT generation and verification using **HMAC-SHA256 (HS256)**:

```nextviper
import std.crypto
import std.time

let JWT_SECRET = "production_cluster_signing_key_991823"

// 1. Issue JWT Token
let claims = {
    "sub": "usr_48291",
    "role": "admin",
    "iat": time.now(),
    "exp": time.now() + 86400
}

let token = crypto.jwt_encode(claims, JWT_SECRET)

// 2. Decode and Verify Signature
let verified_claims = crypto.jwt_decode(token, JWT_SECRET)
if verified_claims == nil:
    // Signature mismatch or invalid token format
    io.print("Unauthorized access attempt!")
```

---

## 3. SQL Injection Defense

The `std.db` engine isolates query templates from variable arguments:
- Parameters are passed as typed array buffers.
- Positional binding (`$1, $2, ...`) guarantees input data is never evaluated as SQL syntax.

---

## 4. Cross-Origin Resource Sharing (CORS)

NextViper's HTTP server engine handles CORS automatically:
- Preflight `OPTIONS` requests receive immediate HTTP 204 responses.
- Standard CORS response headers are configurable per route or globally via middleware.
