#!/usr/bin/env bash
# CPU only: the library is opened for its exported doors; no model, no GPU.
set -uo pipefail
R=$(cd "$(dirname "$0")/../.." && pwd)
D=$R/experiments/EXP-2026-11-19-1459-install-layout-cpu
BIN=$R/builds/flint-acc/bin
OUT=$D/out; mkdir -p "$OUT"; LOG=$OUT/verify.log
{
    echo "# command: gcc layout.c -ldl; ./layout $BIN/libllama.so.0"
    gcc -O1 -o "$OUT/layout" "$D/layout.c" -ldl || exit 1
    LD_LIBRARY_PATH=$BIN "$OUT/layout" "$BIN/libllama.so.0"; rc=$?
    echo "exit=$rc"
    exit $rc
} 2>&1 | tee "$LOG"
rc=${PIPESTATUS[0]}
[ "$rc" -eq 0 ] || { echo "FAIL: an install layout rule changed"; exit 1; }
grep -q "failures=0" "$LOG" || { echo "FAIL: the counts disagree with the verdict"; exit 1; }
echo "PASS: kind, destination, versioned name, guard and version text all hold" | tee -a "$LOG"
