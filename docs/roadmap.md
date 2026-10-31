# NextViper engineering roadmap

NextViper remains experimental. Finish correctness and observable error behavior
before adding execution engines or advertising production use.

## Next five priorities

1. **Ownership and cycles (High):** design cycle collection or explicit lifetime
   boundaries for function/environment and collection cycles; run leak-enabled
   sanitizers on closure-heavy REPL/module workloads. Preserve closures.
2. **Backend contract (High):** expand differential tests for explicit nil slice
   bounds, evaluation order, builtin validation and errors. Add structured skip
   reasons for unsupported imports/captures/native slicing; never count as pass.
3. **Parser and tooling fuzzing (High):** add coverage-guided inputs for annotations,
   expressions, indentation preprocessing and LSP; enforce time/resource limits.
   Extend the generic-nesting crash regression to every recursive entry point.
4. **Security review (High):** independently review archive extraction, package
   authentication, process/network resource limits, HTTP and database boundaries.
   Untrusted language programs still require OS isolation.
5. **Reproducible release validation (Medium):** run the full suite with a dedicated
   PostgreSQL database, available Vulkan device and both Linux CI architectures;
   retain artifacts and dependency/toolchain provenance. Test packaged installation
   and confirm hosted CI before approving a prerelease. No automatic publication.

## Scoped beginner tasks

- Add an asserted CLI regression for one missing file/source-location diagnostic.
- Add a string/raw-string/triple-string grammar fixture using a real TextMate
  tokenizer, documenting its dependencies and exact scopes.
- Add a documented small module application with stdin/stdout and error checks.
- Check documentation links and make the existing mandatory dependencies explicit
  in all platform guides.

## Later work

Separate optional database/GPU dependencies from core scripts; define a stable
module API; decide persistent bytecode format/versioning before implementing it;
consider native slice support after the shared contract is stable; expand the
language specification from observed tests. Language exceptions remain
unimplemented. Windows/macOS and additional CPU/GPU targets need actual build,
runtime and packaging evidence before support claims.

Linguist recognition depends on genuine independent adoption and upstream review,
not a release milestone under this project's control. See the
[submission-readiness draft](linguist/submission-draft.md).
