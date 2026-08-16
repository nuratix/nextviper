# NextViper Database Developer Guide

This guide walks through connecting NextViper backend applications to PostgreSQL databases, executing queries, running migrations, and handling transactions safely.

---

## 1. Connecting to PostgreSQL

```nextviper
import std.db
import std.env

// Load environment variables
env.load(".env")
let db_url = env.require("DATABASE_URL")

let db_client = db.postgres(db_url)
```

---

## 2. Parameterized Queries

Always pass query variables as an array in the second parameter to prevent SQL injection:

```nextviper
// SELECT query with $1 and $2
let search_term = "Enterprise%"
let min_price = 1000.0

let results = db_client.query(
    "SELECT id, name, price, stock FROM products WHERE name LIKE $1 AND price >= $2",
    [search_term, min_price]
)

io.print("Found " + str(len(results["rows"])) + " matching products.")
```

---

## 3. Atomic Transactions

Transactions guarantee that either all database modifications succeed together or none take effect:

```nextviper
fn transfer_funds(tx):
    tx.execute("UPDATE wallets SET balance = balance - $1 WHERE id = $2", [50.0, 1])
    tx.execute("UPDATE wallets SET balance = balance + $1 WHERE id = $2", [50.0, 2])

db_client.transaction(transfer_funds)
```

---

## 4. Connection Cleanup

Always close database handles when shutting down worker processes:

```nextviper
db_client.close()
```
