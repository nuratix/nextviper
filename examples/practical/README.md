# Small interpreter applications

Build NextViper following [the installation guide](../../INSTALLATION.md).
From the repository root, using `bin/nextviper` (or the CMake binary):

```sh
printf '+\n20\n22\n' | bin/nextviper run examples/practical/calculator.nv
# 42
printf 'hello\n\nworld\n' | bin/nextviper run examples/practical/text_summary.nv
# bytes: 13
# nonempty lines: 2
PORT=8080 bin/nextviper run examples/practical/health_server.nv
# In another terminal:
curl http://127.0.0.1:8080/health
# {"ok":true} (JSON whitespace is not significant)
```

Calculator input is an operator and two integer operands, one per line. Unsupported
operators and division by zero fail with a nonzero status. Input parsing follows
the experimental `int()` builtin; this is not a general expression evaluator.
The text utility counts bytes, not Unicode code points, and holds input in memory.
Redirect a file into stdin to process it. The HTTP server is sequential and binds
only to loopback; it has no authentication, TLS, persistence, or deployment claims.
Use Ctrl-C to stop it. These module-based examples require the interpreter; VM
and native module support are incomplete.

Automated checks, including a real HTTP request and cleanup:

```sh
NEXTVIPER_BIN="$PWD/build-cmake/bin/nextviper" python3 tests/test_practical_examples.py
```
