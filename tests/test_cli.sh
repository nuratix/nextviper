#!/usr/bin/env bash
set -e

BIN="./bin/nextviper"

if [ ! -f "$BIN" ]; then
    echo "Binary $BIN not found! Compile first."
    exit 1
fi

echo "Testing NextViper CLI..."

# Test 1: --version
echo "1. Checking --version"
VERSION_OUT=$($BIN --version)
if [[ "$VERSION_OUT" != *"NextViper 0.1.0"* ]]; then
    echo "FAIL: unexpected version output: $VERSION_OUT"
    exit 1
fi
echo "  ✓ PASS: version output correct"

# Test 2: --help
echo "2. Checking --help"
HELP_OUT=$($BIN --help)
if [[ "$HELP_OUT" != *"Usage:"* ]]; then
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
echo "4. Checking syntax validation"
CHECK_OUT=$($BIN check examples/hello_world.nv)
if [[ "$CHECK_OUT" != *"Syntax check passed"* ]]; then
    echo "FAIL: syntax check failed"
    exit 1
fi
echo "  ✓ PASS: syntax check passed"

# Test 5: run command
echo "5. Checking file execution"
RUN_OUT=$($BIN run examples/hello_world.nv)
if [[ "$RUN_OUT" != *"Hello, World! Welcome to NextViper."* ]]; then
    echo "FAIL: unexpected output: $RUN_OUT"
    exit 1
fi
echo "  ✓ PASS: hello_world.nv executed successfully"

# Test 6: parse AST command
echo "6. Checking AST parser"
PARSE_OUT=$($BIN parse examples/hello_world.nv)
if [[ "$PARSE_OUT" != *"Program:"* ]]; then
    echo "FAIL: AST output missing Program:"
    exit 1
fi
echo "  ✓ PASS: AST parsing succeeded"

# Test 7: Error handling with exit code
echo "7. Checking error exit codes"
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
