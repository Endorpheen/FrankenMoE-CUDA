#!/usr/bin/env bash
# CPU only: no model, no GPU. The doors are compared with the native stages they replace.
set -uo pipefail
R=$(cd "$(dirname "$0")/../.." && pwd)
D=$R/experiments/EXP-2026-11-19-1447-sampler-door-differential
BIN=$R/builds/flint-acc/bin
OUT=$D/out; mkdir -p "$OUT"; LOG=$OUT/verify.log
VERDICT=0
{
    echo "# command: gcc diff.c; run with the doors on and with GGML_GOOSE_SAMPLER_TOP_P=0"
    gcc -O1 -o "$OUT/diff" "$D/diff.c" -I "$R/vendor/flint/include" -I "$R/vendor/flint/ggml/include" \
        -L "$BIN" -lllama -lggml-base -lm -Wl,-rpath,"$BIN" || { VERDICT=1; exit 1; }
    LD_LIBRARY_PATH=$BIN                            "$OUT/diff" 40 > "$OUT/goose.txt"  2>"$OUT/goose.err"
    LD_LIBRARY_PATH=$BIN GGML_GOOSE_SAMPLER_TOP_P=0 "$OUT/diff" 40 > "$OUT/native.txt" 2>"$OUT/native.err"
    head -2 "$OUT/goose.txt"; head -2 "$OUT/native.txt"
    # The verdict is the comparison's, never a grep's: a grep that finds nothing exits 1 and would report a failure for
    # a passing run, which happened twice in this project's own checks.
    if ! python3 "$D/compare.py" "$OUT/goose.txt" "$OUT/native.txt"; then VERDICT=1; fi
    echo "FG171_TOP_P lines with the doors on: $(grep -c FG171_TOP_P "$OUT/goose.err" || true)"
    echo "FG171_TOP_P lines with the rollback:  $(grep -c FG171_TOP_P "$OUT/native.err" || true)"
} 2>&1 | tee "$LOG"
grep -q "FG171_TOP_P lines with the doors on: 0" "$LOG" && { echo "FAIL: the door was never reached, so nothing was compared"; exit 1; }
[ "$VERDICT" -eq 0 ] || { echo "FAIL: the doors disagree with the native stages"; exit 1; }
echo "PASS: every selection matches and the kept sets agree up to the threshold boundary" | tee -a "$LOG"
