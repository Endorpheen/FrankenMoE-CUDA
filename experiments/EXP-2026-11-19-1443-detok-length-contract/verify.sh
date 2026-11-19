#!/usr/bin/env bash
# CPU only: vocabulary load, no weights, no inference.
set -uo pipefail
R=$(cd "$(dirname "$0")/../.." && pwd)
D=$R/experiments/EXP-2026-11-19-1443-detok-length-contract
BIN=$R/builds/flint-acc/bin
MODEL=${MODEL:-/home/end0/AI/LocalAI/Models/Apex-35B-V1.0-Q4_K_M.gguf}
OUT=$D/out; mkdir -p "$OUT"; LOG=$OUT/verify.log
{
    echo "# command: gcc regress.c; ./regress $MODEL"
    gcc -O1 -o "$OUT/regress" "$D/regress.c" -I "$R/vendor/flint/include" -I "$R/vendor/flint/ggml/include" \
        -L "$BIN" -lllama -lggml-base -Wl,-rpath,"$BIN" || exit 1
    LD_LIBRARY_PATH=$BIN "$OUT/regress" "$MODEL"; rc=$?
    echo "exit=$rc"
    exit $rc
} 2>&1 | tee "$LOG"
rc=${PIPESTATUS[0]}
[ "$rc" -eq 0 ] || { echo "FAIL: a short buffer was overflowed or the contract changed"; exit 1; }
echo "PASS: every short-buffer call returns -needed and writes nothing past its buffer"
