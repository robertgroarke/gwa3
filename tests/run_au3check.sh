#!/bin/bash
# Au3Check validation for Froggy function extraction
# Baseline: 191 errors (all pre-existing)
# Usage: bash tests/run_au3check.sh

AU3CHECK="/c/Program Files (x86)/AutoIt3/Au3Check.exe"
LIB="c:/Users/Robert/Documents/GWA Censured X BotsHub/GWA Censured/lib"
FROGGY="c:/Users/Robert/Documents/GWA Censured X BotsHub/GWA Censured/Froggy_HM_v1.6.au3"
BASELINE=191

echo "=== Au3Check Validation ==="
ERRORS=$("$AU3CHECK" -q -I "$LIB" -I "$LIB/botshub" -I "$LIB/custom" -I "/c/Program Files (x86)/AutoIt3/Include" "$FROGGY" 2>&1 | grep -c "error:")
echo "Errors: $ERRORS (baseline: $BASELINE)"

if [ "$ERRORS" -le "$BASELINE" ]; then
    echo "PASS: Error count at or below baseline"

    # Show real errors (excluding pre-existing noise)
    REAL=$("$AU3CHECK" -q -I "$LIB" -I "$LIB/botshub" -I "$LIB/custom" -I "/c/Program Files (x86)/AutoIt3/Include" "$FROGGY" 2>&1 | grep "error:" | grep -v "already defined\|previously declared\|syntax error\|Statement cannot\|Program Files\|DllStructSetData" | wc -l)
    echo "Real errors: $REAL"

    if [ "$REAL" -gt 0 ]; then
        echo "--- Real errors ---"
        "$AU3CHECK" -q -I "$LIB" -I "$LIB/botshub" -I "$LIB/custom" -I "/c/Program Files (x86)/AutoIt3/Include" "$FROGGY" 2>&1 | grep "error:" | grep -v "already defined\|previously declared\|syntax error\|Statement cannot\|Program Files\|DllStructSetData"
    fi
    exit 0
else
    echo "FAIL: Error count increased by $((ERRORS - BASELINE))"
    echo "--- New errors ---"
    "$AU3CHECK" -q -I "$LIB" -I "$LIB/botshub" -I "$LIB/custom" -I "/c/Program Files (x86)/AutoIt3/Include" "$FROGGY" 2>&1 | grep "error:" | grep -v "already defined\|previously declared\|syntax error\|Statement cannot\|Program Files\|DllStructSetData"
    exit 1
fi
