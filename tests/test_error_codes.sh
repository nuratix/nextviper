#!/usr/bin/env bash
set -e

BIN="./bin/nextviper"

if [ ! -f "$BIN" ]; then
    echo "Binary $BIN not found! Compile first."
    exit 1
fi

echo "=================================================="
echo "  NEXTVIPER ERROR SYSTEM INTEGRATION TESTS        "
echo "=================================================="

# Test 1: Unknown Identifier (NV1001)
echo "1. Checking Unknown Identifier (NV1001)"
TMP_ERR1="/tmp/err_test_nv1001_$$.nv"
echo "let result = undefined_var + 10" > "$TMP_ERR1"
set +e
OUT1=$($BIN check "$TMP_ERR1" 2>&1)
set -e
rm -f "$TMP_ERR1"

if [[ "$OUT1" != *"error[NV1001]"* ]]; then
    echo "FAIL: Expected error[NV1001], got: $OUT1"
    exit 1
fi
if [[ "$OUT1" != *"https://nextviper.nuratix.com/docs/errors/unknown-identifier"* ]]; then
    echo "FAIL: Missing documentation URL in NV1001 output: $OUT1"
    exit 1
fi
echo "  ✓ PASS: NV1001 error code and documentation link verified"

# Test 2: Type Mismatch (NV1003)
echo "2. Checking Type Mismatch (NV1003)"
TMP_ERR2="/tmp/err_test_nv1003_$$.nv"
echo 'let x: int = "string_value"' > "$TMP_ERR2"
set +e
OUT2=$($BIN check "$TMP_ERR2" 2>&1)
set -e
rm -f "$TMP_ERR2"

if [[ "$OUT2" != *"error[NV1003]"* ]]; then
    echo "FAIL: Expected error[NV1003], got: $OUT2"
    exit 1
fi
if [[ "$OUT2" != *"https://nextviper.nuratix.com/docs/errors/type-mismatch"* ]]; then
    echo "FAIL: Missing documentation URL in NV1003 output: $OUT2"
    exit 1
fi
echo "  ✓ PASS: NV1003 error code and documentation link verified"

# Test 3: JSON Format Output with Documentation URL
echo "3. Checking check --format=json documentation field"
TMP_ERR3="/tmp/err_test_json_$$.nv"
echo "let total = unknown_id + 5" > "$TMP_ERR3"
set +e
JSON_OUT=$($BIN check "$TMP_ERR3" --format=json 2>&1)
set -e
rm -f "$TMP_ERR3"

if [[ "$JSON_OUT" != *"NV1001"* ]] || [[ "$JSON_OUT" != *"https://nextviper.nuratix.com/docs/errors/unknown-identifier"* ]]; then
    echo "FAIL: Expected JSON documentation field, got: $JSON_OUT"
    exit 1
fi
echo "  ✓ PASS: JSON machine-readable error output contains documentation field"

# Test 4: File Not Found (NV2002)
echo "4. Checking File Not Found (NV2002)"
set +e
OUT4=$($BIN run /tmp/non_existent_file_$$.nv 2>&1)
set -e

if [[ "$OUT4" != *"error[NV2002]"* ]]; then
    echo "FAIL: Expected error[NV2002], got: $OUT4"
    exit 1
fi
if [[ "$OUT4" != *"https://nextviper.nuratix.com/docs/errors/file-not-found"* ]]; then
    echo "FAIL: Missing documentation URL in NV2002 output: $OUT4"
    exit 1
fi
echo "  ✓ PASS: NV2002 CLI file error code and documentation link verified"

echo "=================================================="
echo "  🎉 ALL ERROR SYSTEM INTEGRATION TESTS PASSED!   "
echo "=================================================="
