# Troubleshooting the experimental toolchain

- **CMake not found / missing headers:** install the build tools and all development
  libraries in [INSTALLATION.md](../INSTALLATION.md). Vulkan, libpq and libcurl
  headers are mandatory even when your program does not use these modules.
- **Wrong executable tested:** CMake writes to `build-cmake/bin`; Make writes to
  `bin`. Set `NEXTVIPER_BIN` to an absolute executable path for Python integration
  tests. `ctest --test-dir build-cmake --output-on-failure` selects its built binary.
- **VM captures/imports or native slices rejected:** run the program using
  `nextviper run file.nv --tree`; consult [CURRENT_STATUS.md](../CURRENT_STATUS.md).
  A compiler diagnostic for unsupported syntax is not successful execution.
- **PostgreSQL tests skipped:** set `NEXTVIPER_TEST_DATABASE_URL` to a dedicated
  disposable PostgreSQL database. Tests modify their fixtures. Without a database
  connection these tests cannot establish persistence or SQL behavior.
- **No Vulkan device:** building with headers/loader does not prove GPU execution.
  Test GPU workloads on supported hardware and record device/driver details.
- **Sanitizer runtime will not initialize:** record the exact error and use UBSan
  separately to investigate undefined behavior. Do not describe that as an ASan
  pass. Reference cycles remain a known leak issue; see [TESTING.md](../TESTING.md).
- **HTTP example port occupied:** set another `PORT` for the loopback health example.
  The examples are sequential development servers, not public production services.
- **An unrecognized language feature:** `try/catch/throw`, persistent bytecode files
  and universal backend equivalence are not implemented contracts. Use the
  specification and test suite, and report the smallest reproducing program.

Report the commit, operating system/CPU, compiler and dependency versions, exact
command, exit status, stdout/stderr and a minimal `.nv` file. Remove passwords,
tokens and private database URLs before sharing logs.
