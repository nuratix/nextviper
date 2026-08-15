#!/usr/bin/env bash
set -e

BIN="./bin/nextviper"
LSP_BIN="./bin/nextviper-lsp"

if [ ! -f "$BIN" ]; then
    echo "Binary $BIN not found! Compile first."
    exit 1
fi

echo "Testing NextViper CLI Tooling..."

# Test 1: --version
echo "1. Checking --version"
VERSION_OUT=$($BIN --version)
if [[ "$VERSION_OUT" != *"NextViper 1.0.0"* ]]; then
    echo "FAIL: unexpected version output: $VERSION_OUT"
    exit 1
fi
echo "  ✓ PASS: version output correct"

# Test 2: --help
echo "2. Checking --help"
HELP_OUT=$($BIN --help)
if [[ "$HELP_OUT" != *"USAGE:"* ]] && [[ "$HELP_OUT" != *"Usage:"* ]]; then
    echo "FAIL: unexpected help output"
    exit 1
fi
echo "  ✓ PASS: help output correct"

# Test 3: eval mode
echo "3. Checking eval (-e)"
EVAL_OUT=$($BIN -e 'let a = 20; let b = 22; print(a + b);')
if [[ "$EVAL_OUT" != *"42"* ]]; then
    echo "FAIL: expected '42', got: $EVAL_OUT"
    exit 1
fi
echo "  ✓ PASS: eval output correct (42)"

# Test 4: check command
echo "4. Checking syntax & type validation"
CHECK_OUT=$($BIN check examples/hello_world.nv)
if [[ "$CHECK_OUT" != *"Check passed"* ]]; then
    echo "FAIL: check failed: $CHECK_OUT"
    exit 1
fi
echo "  ✓ PASS: check command succeeded"

# Test 5: check --format=json
echo "5. Checking check --format=json"
JSON_OUT=$($BIN check examples/hello_world.nv --format=json)
if [[ "$JSON_OUT" != *"["* ]]; then
    echo "FAIL: expected JSON output"
    exit 1
fi
echo "  ✓ PASS: check --format=json succeeded"

# Test 6: run command
echo "6. Checking file execution"
RUN_OUT=$($BIN run examples/hello_world.nv)
if [[ "$RUN_OUT" != *"Hello, World! Welcome to NextViper."* ]]; then
    echo "FAIL: unexpected output: $RUN_OUT"
    exit 1
fi
echo "  ✓ PASS: hello_world.nv executed successfully"

# Test 7: Formatter command
echo "7. Checking fmt command"
TMP_FMT="/tmp/test_fmt_$$.nv"
echo "let   x=10+20" > "$TMP_FMT"
$BIN fmt "$TMP_FMT"
FMT_CONTENT=$(cat "$TMP_FMT")
if [[ "$FMT_CONTENT" != *"let x = 10 + 20"* ]]; then
    echo "FAIL: fmt failed to format file: $FMT_CONTENT"
    rm -f "$TMP_FMT"
    exit 1
fi
rm -f "$TMP_FMT"
echo "  ✓ PASS: fmt in-place formatting succeeded"

# Test 8: Formatter --check
echo "8. Checking fmt --check"
TMP_FMT="/tmp/test_fmt_clean_$$.nv"
echo "let x = 10 + 20" > "$TMP_FMT"
$BIN fmt "$TMP_FMT" --check > /dev/null 2>&1
rm -f "$TMP_FMT"
echo "  ✓ PASS: fmt --check passed on clean code"

# Test 9: Build command
echo "9. Checking build command"
BUILD_OUT=$($BIN build examples/modules_example.nv -o build/test_cli_build.nvc)
if [[ "$BUILD_OUT" != *"Built bytecode package"* ]] && [[ "$BUILD_OUT" != *"Successfully compiled and built"* ]]; then
    echo "FAIL: build command failed: $BUILD_OUT"
    exit 1
fi
echo "  ✓ PASS: build command succeeded"

# Test 10: Package command
echo "10. Checking package command"
PKG_OUT=$($BIN package help)
if [[ "$PKG_OUT" != *"NextViper Package Manager"* ]]; then
    echo "FAIL: package help failed"
    exit 1
fi
echo "  ✓ PASS: package command succeeded"

# Test 11: Info command
echo "11. Checking info command"
INFO_OUT=$($BIN info)
if [[ "$INFO_OUT" != *"NextViper Toolchain"* ]]; then
    echo "FAIL: info output incorrect"
    exit 1
fi
echo "  ✓ PASS: info command succeeded"

# Test 12: LSP Binary --version
echo "12. Checking nextviper-lsp binary"
if [ -f "$LSP_BIN" ]; then
    LSP_OUT=$($LSP_BIN --version)
    if [[ "$LSP_OUT" != *"nextviper-lsp"* ]]; then
        echo "FAIL: lsp binary failed"
        exit 1
    fi
    echo "  ✓ PASS: nextviper-lsp binary verified"
fi

# Test 13: Error handling with non-zero exit code
echo "13. Checking error exit codes"
set +e
$BIN run non_existent_file.nv > /dev/null 2>&1
EXIT_CODE=$?
set -e
if [ $EXIT_CODE -eq 0 ]; then
    echo "FAIL: expected non-zero exit code on missing file"
    exit 1
fi
echo "  ✓ PASS: non-zero exit code on missing file"

echo -e "\n\033[1;32mAll CLI Integration Tests Passed!\033[0m"
