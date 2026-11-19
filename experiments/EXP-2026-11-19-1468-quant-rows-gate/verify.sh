#!/usr/bin/env bash
# The quant row gate: every type the enumerator has, against the native sizes.
#
# It reports two different things and they must not be confused: whether the ports are right where they answer, and whether
# they answer at all. A wrong size fails the gate; a type no port answers is printed as a coverage gap, because deleting a
# native body that is the only implementation of a type is what risk R-1 forbids.
set -uo pipefail
R=$(cd "$(dirname "$0")/../.." && pwd)
D=$R/experiments/EXP-2026-11-19-1468-quant-rows-gate
G=${GOOSE_COMPILER:-/home/end0/AI/LlamaServer/toolchain/goose-2026.3-string/goose}
BIN=$R/builds/flint-acc/bin
OUT=$D/out; mkdir -p "$OUT"; LOG=$OUT/verify.log
{
    echo "# command: goose --standalone per module; gcc gate.c quant-rows.c quant-rows2.o host.c"
    for m in quant-rows quant-rows2; do
        "$G" --standalone --header "$OUT/$m.h" -o "$OUT/$m.c" "$R/goose/model/$m.goose" >/dev/null || exit 1
    done
    gcc -O1 -c -Dgoose_init=goose_init2 -o "$OUT/quant-rows2.o" "$OUT/quant-rows2.c" || exit 1
    cp "$D/gate.c" "$OUT/gate.c"; cp "$D/host.c" "$OUT/host.c"
    gcc -O1 -o "$OUT/gate" "$OUT/gate.c" "$OUT/quant-rows.c" "$OUT/quant-rows2.o" "$OUT/host.c" \
        -I "$R/vendor/flint/ggml/include" -L "$BIN" -lggml-base -lm -Wl,-rpath,"$BIN" || exit 1
    LD_LIBRARY_PATH=$BIN "$OUT/gate"; rc=$?
    echo "exit=$rc"
    exit $rc
} 2>&1 | tee "$LOG"
rc=${PIPESTATUS[0]}
[ "$rc" -eq 0 ] || { echo "FAIL: a ported row size disagrees with the native one"; exit 1; }
grep -q "row_size_failures=0" "$LOG" || { echo "FAIL: the counts disagree with the verdict"; exit 1; }
gaps=$(grep -c NEITHER "$LOG")
echo "PASS: every type the ports answer has the native sizes ($(grep -oE 'types=[0-9]+ port1=[0-9]+ port2=[0-9]+' "$LOG" | head -1)); $gaps type(s) are answered by neither and block removal" | tee -a "$LOG"
