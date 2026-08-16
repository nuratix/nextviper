# NextViper Configuration & Environment Management

NextViper follows the **Twelve-Factor App** methodology for configuration, utilizing environment variables and `.env` files via `std.env`.

---

## 1. The `std.env` Module

```nextviper
import std.env

// 1. Load configuration from .env file
env.load(".env")

// 2. Fetch optional variable with fallback default
let port = env.get("PORT", "8080")
let debug_mode = env.get("DEBUG", "false")

// 3. Require mandatory variable (throws error if unset)
let database_url = env.require("DATABASE_URL")
let jwt_secret = env.require("JWT_SECRET")

// 4. Set environment variable dynamically
env.set("APP_STATUS", "ready")
```

---

## 2. Production Deployment Best Practices

- Store sensitive credentials (database passwords, API keys, encryption secrets) exclusively in environment variables.
- Commit `.env.example` to version control and ignore `.env` via `.gitignore`.
- Use `env.require()` at application boot to fail fast if critical configuration is missing.
