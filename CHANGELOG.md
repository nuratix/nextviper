# Changelog

## Unreleased — proposed 1.0.1-rc.1

- Reject invalid Unicode scalar escapes and classify lexer characters as ASCII.
- Bound recursive generic type parsing instead of overflowing the process stack.
- Share safe interpreter/VM slicing: reverse defaults, clamped bounds, full-range
  strides, and target resizing during bound evaluation.
- Strengthen native equivalence checks to compare complete observable results,
  isolate temporary binaries, bound subprocess runtimes and honor NEXTVIPER_BIN.
- Add practical calculator, text summary and loopback HTTP applications with tests.
- Correct grammar keyword/operator patterns and contributor prerequisites.
- Add measured benchmark tooling, engineering audit, validation and roadmap.

This is an experimental prerelease recommendation, not a published release.
The compiler version string remains unchanged. Existing cycles, backend gaps and
unverified platforms remain limitations. Development was AI-assisted.
