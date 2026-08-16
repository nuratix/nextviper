# NextViper Database Architecture Specification

NextViper provides a high-reliability, enterprise-grade database subsystem via `std.db` designed for relational databases like PostgreSQL.

---

## 1. Core Principles

1. **Mandatory Parameterized Queries**: All SQL queries supporting dynamic inputs enforce positional parameter binding (`$1, $2, ...` or `?`). Raw string interpolation of user input is strictly discouraged to eliminate SQL injection vulnerabilities.
2. **Connection Pooling**: Thread-safe pool management provides connection reuse, configurable min/max connections, health checks, and lifecycle recycling.
3. **Atomic Transaction Guarantees**: ACID transactions with automatic `BEGIN`, `COMMIT`, and rollback on error.

---

## 2. PostgreSQL Client Model

```nextviper
import std.db

let client = db.postgres("postgres://user:password@localhost:5432/app_db")

// Safe Parameterized Query
let res = client.query("SELECT * FROM users WHERE active = $1 AND role = $2", [true, "admin"])
for user in res["rows"]:
    io.print("User: " + user["username"])

// Data Manipulation
let result = client.execute("UPDATE accounts SET balance = balance + $1 WHERE id = $2", [250.0, 101])
io.print("Affected rows: " + str(result["affected_rows"]))

// Transaction Management
client.transaction(fn(tx):
    tx.execute("UPDATE accounts SET balance = balance - $1 WHERE id = $2", [100.0, 101])
    tx.execute("UPDATE accounts SET balance = balance + $1 WHERE id = $2", [100.0, 202])
)

client.close()
```

---

## 3. SQL Injection Defense

The NextViper SQL parameter binder intercepts all argument arrays passed to `client.query()` and `client.execute()`, sanitizing literals, escaping binary sequences, and validating type boundaries before query execution.
