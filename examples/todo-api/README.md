# A small NextViper backend

This single-user API stores tasks in PostgreSQL and supports create, list, read,
update, and delete. It uses parameterized SQL, validates JSON fields, requires a
bearer token on task routes, and survives application restarts. `/health` checks
the database without requiring a token. Listing returns the latest 100 tasks.

Build and install NextViper using the repository README. Create a PostgreSQL
application database and a role with permission to create tables in that database.
The app creates only its `nextviper_tasks` table on startup.

```sh
export DATABASE_URL='postgresql://app:your-password@127.0.0.1:5432/tasks?connect_timeout=5'
export API_TOKEN="$(openssl rand -hex 32)"
nextviper run examples/todo-api/app.nv
```

The server listens on `127.0.0.1:8080`. Set `HOST` and `PORT` to change this.
In a second terminal, set `API_TOKEN` to the same value and try:

```sh
curl http://127.0.0.1:8080/health
curl http://127.0.0.1:8080/tasks \
  -H "Authorization: Bearer $API_TOKEN" \
  -H 'Content-Type: application/json' \
  -d '{"title":"My first NextViper task"}'

curl http://127.0.0.1:8080/tasks -H "Authorization: Bearer $API_TOKEN"
curl -X PUT http://127.0.0.1:8080/tasks/1 \
  -H "Authorization: Bearer $API_TOKEN" \
  -H 'Content-Type: application/json' \
  -d '{"title":"My first NextViper task","done":true}'
curl -X DELETE http://127.0.0.1:8080/tasks/1 -H "Authorization: Bearer $API_TOKEN"
```

Use the id returned by your create request. Restarting the app with the same
`DATABASE_URL` preserves tasks. Stop the foreground process with Ctrl+C.

For a small hosted deployment, run it as an unprivileged service with a dedicated
database role, persistent PostgreSQL storage, and a TLS reverse proxy. Keep the
backend bound to loopback and pass requests from the proxy. Configure request
size/time limits and rate limiting at that proxy. Keep the token and database URL
in the service environment. Back up the database and test restores.

This example has one shared token, no user accounts or per-user authorization,
and no browser login flow. The HTTP server processes requests sequentially;
long-running handlers block other requests. It is intended for small tools and
evaluation, with no throughput or availability guarantee. The interpreter is the
supported execution path for this example; the VM/native backend do not support
its modules.

Run the persistence and CRUD checks against a dedicated test database:

```sh
NEXTVIPER_TEST_DATABASE_URL='postgresql://app:your-password@127.0.0.1:5432/test_tasks?connect_timeout=5' \
  python3 tests/test_backend.py
```
