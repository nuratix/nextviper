# Native compiler status

The experimental native pipeline is AST → register IR → generated C++20 → host compiler → executable. Runtime values have explicit type tags; function calls preserve dynamic values and captured variable cells. The generated program uses the C++ standard library.

Supported regression cases include fractional function calls, primitive math, strings, heterogeneous arrays/maps, recursion, closures, collection cycles in printing, ordinary loops, range calls with steps, and short-circuit logical operators. Run the tests to validate your checkout; this list is not a complete equivalence guarantee.

Native libraries are a subset of interpreter libraries. Unsupported modules, builtins, slicing, indexed compound assignments and closure declarations inside loops must fail with a diagnostic. C++20 tooling and Linux/POSIX assumptions remain. Reference cycles can retain memory. There is no general performance or production-readiness guarantee.

`build` produces a native executable by default. `--bytecode` explicitly fails because artifact serialization/loading is unfinished. The separate VM can still run in-memory bytecode with `run --vm`.
