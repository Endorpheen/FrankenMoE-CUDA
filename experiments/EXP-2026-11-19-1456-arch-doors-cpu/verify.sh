#!/usr/bin/env bash
# CPU only: the library is opened for its exported doors, no model is loaded and no GPU is touched.
set -uo pipefail
R=$(cd "$(dirname "$0")/../.." && pwd)
D=$R/experiments/EXP-2026-11-19-1456-arch-doors-cpu
BIN=$R/builds/flint-acc/bin
OUT=$D/out; mkdir -p "$OUT"; LOG=$OUT/verify.log
{
    echo "# command: gcc doors.c -ldl; ./doors $BIN/libllama.so.0"
    gcc -O1 -o "$OUT/doors" "$D/doors.c" -ldl || exit 1
    LD_LIBRARY_PATH=$BIN "$OUT/doors" "$BIN/libllama.so.0"; rc=$?
    echo "exit=$rc"
    exit $rc
} 2>&1 | tee "$LOG"
rc=${PIPESTATUS[0]}
[ "$rc" -eq 0 ] || { echo "FAIL: a door disagrees with the registry it must describe"; exit 1; }
grep -q "round_trip_failures=0 duplicate_rows=0 unknown_refused=6/6 ver_checked=7 ver_failures=0" "$LOG" \
    || { echo "FAIL: the counts do not agree with the verdict"; exit 1; }
echo "PASS: every row round-trips through its name, unknown names are refused, and the version table holds" | tee -a "$LOG"
