#!/usr/bin/env bash
# The gate for the joint QG split. The module is compiled on its own against a capture host, so the numbers it would hand
# the graph builder are recorded and compared with the native expression instead of being trusted.
set -uo pipefail
R=$(cd "$(dirname "$0")/../.." && pwd)
D=$R/experiments/EXP-2026-11-19-1467-qg-split-gate
G=${GOOSE_COMPILER:-/home/end0/AI/LlamaServer/toolchain/goose-2026.3-string/goose}
OUT=$D/out; mkdir -p "$OUT"; LOG=$OUT/verify.log
{
    echo "# command: goose --standalone --header qg.h -o qg.c goose/model/qgate-split.goose; gcc host.c qg.c"
    "$G" --standalone --header "$OUT/qg.h" -o "$OUT/qg.c" "$R/goose/model/qgate-split.goose" >/dev/null || exit 1
    sed -e 's|experiments/EXP-2026-11-19-1467-qg-split-gate/qg.c|'"$OUT"'/qg.c|' "$D/host.c" > "$OUT/host.c"
    cp "$OUT/qg.h" "$D/qg.h.copy" 2>/dev/null || true
    gcc -O1 -o "$OUT/gate" "$OUT/host.c" "$OUT/qg.c" || exit 1
    "$OUT/gate"; rc=$?
    echo "exit=$rc"
    exit $rc
} 2>&1 | tee "$LOG"
rc=${PIPESTATUS[0]}
rm -f "$D/qg.h.copy"
[ "$rc" -eq 0 ] || { echo "FAIL: the split decision no longer matches the native expression"; exit 1; }
grep -q "shapes=7 refusals=4 failures=0" "$LOG" || { echo "FAIL: the counts disagree with the verdict"; exit 1; }
echo "PASS: strides, shapes and offsets match the native line, and all four zero dimensions refuse loudly" | tee -a "$LOG"
