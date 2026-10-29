# Current project status

NextViper is experimental. The current 1.0.0 version string does not indicate production certification or API stability.

## Implementation scope

| Area | Current scope | Important limits |
| --- | --- | --- |
| Interpreter | Basic syntax, functions, lexical closures, collections, modules | Dynamic library validation and resource bounds are incomplete |
| Type checker | Primitive inference/annotations, assignment mutability, basic control-flow checks | Gradual checking with `any`; incomplete library signatures and custom/generic runtime annotation enforcement |
| VM | In-memory compilation/execution; explicit `run --vm` | No module imports or lexical captures; these fail with an interpreter recommendation. Operand/constant limits are diagnosed; no persistent bytecode artifacts |
| Native compiler | IR to standalone C++20 with tagged values; functions, captured cells, arithmetic, basic collections/control flow | Limited builtins/modules; slicing, indexed compound assignments, closure declarations inside loops, general imports and other unsupported operations must be diagnosed; no performance guarantee |
| Formatter | Conservative line formatting; preserves leading indentation and verifies lexical tokens | Does not repair malformed indentation; unsupported lexical cases may be left unchanged |
| REPL | Retains parsed programs for callable lifetime | Keeps submitted programs until the session ends; no cycle-collecting GC |
| HTTP client | Legacy curl subprocess APIs; new libcurl result/stream APIs with actual headers, verified TLS, deadlines and byte caps | New APIs require libcurl 7.85+; no general application SSRF policy or production certification; see [transport APIs](TRANSPORT_API.md) |
| Interactive processes | POSIX argv-based spawn, separate pipes, bounded I/O and process-group cleanup | Linux ARM64 tested; no OS sandbox, VM/native-AOT support or cross-platform certification |
| Data/AI/GPU/database | Implementations with repository tests | Hardware, workload, live database and deployment behavior need independent validation |
| Package manager | Manifest/lockfile and dependency tooling | External registry infrastructure, supply-chain trust, archive handling and reproducibility need further review |
| Platforms | Linux development/build workflow | Windows/macOS and binary distribution are not established by local Linux tests |

## Trust boundary

NextViper programs run with the operating system permissions of the process. This is **not a sandbox for untrusted programs**. Filesystem, process, network, and package APIs can have external side effects. Run unfamiliar programs in OS-level isolation with resource limits.

Reference counting is not cycle collection. Self-referential collections and function/environment cycles can retain memory. Bounded collection printing avoids one class of recursion crash; it is not a general memory-safety proof. Parser depth, recursion checks and collection caps cover selected paths, not every allocation or CPU workload. Core integer operators now check overflow in the interpreter, VM, and generated native runtime. VM/interpreter equivalence beyond the regression suite still needs broader review.

## Validation policy

Use `make test` or CTest for current results. Tests include independent regressions for the readiness review failures. A passing suite establishes only its tested cases. Record the commit, platform, compiler, dependencies, commands, and limitations for any claim of verification.

No assertion of production readiness, universal memory safety, complete sandboxing, cross-platform buildability, guaranteed reproducibility, or competitive benchmark performance is currently made.

## First release target

Small Linux scripts and sequential HTTP backends are the initial target, using
the interpreter. `examples/todo-api` is a single-user PostgreSQL CRUD app with
token authentication, JSON validation, and restart persistence checks. It is an
example with explicit limits, not a multi-user account system or high-load
server. HTTP malformed JSON/framing, response-header injection, disconnected
clients, and shutdown have dedicated socket regressions.

Linux x86_64 and ARM64 CI supply a PostgreSQL 17 service. Database checks skip
locally unless `NEXTVIPER_TEST_DATABASE_URL` is set; report that skip in any
validation claim. Packaged installs are exercised from an unrelated directory.
The npm package requires an existing native installation. CI definitions and
passing local tests alone do not establish that both hosted jobs have passed.
