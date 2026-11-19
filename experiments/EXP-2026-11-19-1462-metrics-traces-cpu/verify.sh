#!/usr/bin/env bash
# CPU only: the library is opened for its exported doors; no model, no GPU, no trace file is produced.
set -uo pipefail
R=$(cd "$(dirname "$0")/../.." && pwd)
D=$R/experiments/EXP-2026-11-19-1462-metrics-traces-cpu
BIN=$R/builds/flint-acc/bin
OUT=$D/out; mkdir -p "$OUT"; LOG=$OUT/verify.log
{
    echo "# command: gcc traces.c -ldl -lm; ./traces $BIN/libllama.so.0"
    gcc -O1 -o "$OUT/traces" "$D/traces.c" -ldl -lm || exit 1
    LD_LIBRARY_PATH=$BIN "$OUT/traces" "$BIN/libllama.so.0"; rc=$?
    echo "exit=$rc"
    exit $rc
} 2>&1 | tee "$LOG"
rc=${PIPESTATUS[0]}
[ "$rc" -eq 0 ] || { echo "FAIL: a metrics or trace rule changed"; exit 1; }
grep -q "failures=0" "$LOG" || { echo "FAIL: the counts disagree with the verdict"; exit 1; }
echo "PASS: quoting, escaping, the NaN arm, the column widths and a full compute row all hold" | tee -a "$LOG"
