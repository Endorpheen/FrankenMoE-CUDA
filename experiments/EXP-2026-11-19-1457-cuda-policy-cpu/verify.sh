#!/usr/bin/env bash
# CPU only: the library is opened for its exported doors; no model, no GPU.
set -uo pipefail
R=$(cd "$(dirname "$0")/../.." && pwd)
D=$R/experiments/EXP-2026-11-19-1457-cuda-policy-cpu
BIN=$R/builds/flint-acc/bin
OUT=$D/out; mkdir -p "$OUT"; LOG=$OUT/verify.log
{
    echo "# command: gcc policy.c -ldl; ./policy $BIN/libllama.so.0"
    gcc -O1 -o "$OUT/policy" "$D/policy.c" -ldl || exit 1
    LD_LIBRARY_PATH=$BIN "$OUT/policy" "$BIN/libllama.so.0"; rc=$?
    echo "exit=$rc"
    exit $rc
} 2>&1 | tee "$LOG"
rc=${PIPESTATUS[0]}
[ "$rc" -eq 0 ] || { echo "FAIL: a CUDA policy branch changed"; exit 1; }
grep -q "arch_default_cases=7 tgt_flags_cases=6 failures=0" "$LOG" || { echo "FAIL: the counts disagree"; exit 1; }
echo "PASS: every branch of arch_default and tgt_flags behaves as the module states" | tee -a "$LOG"
