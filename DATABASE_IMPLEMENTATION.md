# NextViper Database Implementation (`std.db`)

## Architecture & Real Engine Integration
NextViper standard library `std.db` implements database access using the official PostgreSQL client C library (`libpq`). Fake mock responses and hardcoded dummy records have been completely removed.

### Connecting to PostgreSQL
Connections are established using `db.postgres(config)` or connection string URIs:
```nv
import db

// Object Configuration
let client = db.postgres({
    "host": "127.0.0.1",
    "port": 5432,
    "database": "production_db",
    "user": "postgres",
    "password": "secret_password"
})

// Connection URI format
let client2 = db.postgres("postgresql://postgres:secret@127.0.0.1:5432/production_db")
```

### Connection Error Handling
If PostgreSQL is unreachable, credentials are invalid, or network errors occur, NextViper immediately raises an authentic PostgreSQL error message directly from `PQerrorMessage(conn)` with diagnostic guidance:
```
error[NV100]:
    PostgreSQL Connection Error: connection to server at "127.0.0.1", port 5432 failed: Connection refused
    Is the server running on that host and accepting TCP/IP connections?
```

### Query Execution (`client.query`)
Supports parameterized queries to prevent SQL injection vulnerabilities:
```nv
let result = client.query("SELECT id, username, email FROM users WHERE status = $1", ["active"])
print("Fetched rows count:", result.count)
for user in result.rows {
    print("User:", user["username"], user["email"])
}
```
Return Object Schema:
- `rows`: Array of objects where each key is a column name and value is the string data or `nil` for NULL values.
- `count`: Integer number of rows retrieved.
- `fields`: Array of string column names.
- `sql`: Original SQL query string.

### DML & Statement Execution (`client.execute`)
Executes `INSERT`, `UPDATE`, `DELETE`, `CREATE TABLE`, and other SQL statements:
```nv
let result = client.execute("UPDATE users SET last_login = NOW() WHERE id = $1", [42])
print("Rows modified:", result.affected_rows)
```

### Transaction Management (`client.transaction`)
Executes an atomic transaction block with automatic rollback on error:
```nv
client.transaction(fn(tx) {
    tx.execute("INSERT INTO orders (user_id, amount) VALUES ($1, $2)", [1, 99.50])
    tx.execute("UPDATE accounts SET balance = balance - $1 WHERE id = $2", [99.50, 1])
})
```
- Issues `BEGIN` before callback execution.
- If callback completes successfully, issues `COMMIT`.
- If an error is raised inside the closure, issues `ROLLBACK` and rethrows the error.

### Connection Lifecycle (`client.close`)
Safely closes the underlying connection via `PQfinish(conn)` and cleans up memory under mutex synchronization.
