#!/usr/bin/env bash
# CPU only: the library is opened for its exported doors; no model, no GPU, no network.
set -uo pipefail
R=$(cd "$(dirname "$0")/../.." && pwd)
D=$R/experiments/EXP-2026-11-19-1460-hf-naming-cpu
BIN=$R/builds/flint-acc/bin
OUT=$D/out; mkdir -p "$OUT"; LOG=$OUT/verify.log
{
    echo "# command: gcc naming.c -ldl; ./naming $BIN/libllama.so.0"
    gcc -O1 -o "$OUT/naming" "$D/naming.c" -ldl || exit 1
    LD_LIBRARY_PATH=$BIN "$OUT/naming" "$BIN/libllama.so.0"; rc=$?
    echo "exit=$rc"
    exit $rc
} 2>&1 | tee "$LOG"
rc=${PIPESTATUS[0]}
[ "$rc" -eq 0 ] || { echo "FAIL: a cache naming rule changed"; exit 1; }
grep -q "failures=0" "$LOG" || { echo "FAIL: the counts disagree with the verdict"; exit 1; }
echo "PASS: repo, commit, folder names, their inverse and subpath containment all hold" | tee -a "$LOG"
