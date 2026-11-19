#!/usr/bin/env bash
# Regression for the piece-buffer overlap. CPU only: it loads the vocabulary, not the weights, and performs no inference.
set -euo pipefail
R=$(cd "$(dirname "$0")/../.." && pwd)
E=$R/experiments/EXP-2026-11-19-1441-piece-buffer-regression
BIN=${BIN:-$R/builds/flint-acc/bin}
MODEL=${MODEL:-/home/end0/AI/LocalAI/Models/Apex-35B-V1.0-Q4_K_M.gguf}
OUT=$E/out
mkdir -p "$OUT"
LOG=$OUT/verify.log
{
    echo "# command: g++ -O1 regress.cpp ... && ./piece_regress $MODEL"
    g++ -O1 -o "$OUT/piece_regress" "$E/regress.cpp" \
        -I "$R/vendor/flint/include" -I "$R/vendor/flint/ggml/include" -I "$R/vendor/flint/common" \
        -L "$BIN" -lllama-common -lllama -lggml-base -Wl,-rpath,"$BIN"
    "$OUT/piece_regress" "$MODEL"
} 2>&1 | tee "$LOG"
rc=${PIPESTATUS[0]}
echo "exit=$rc"
[ "$rc" -eq 0 ] || { echo "FAIL: the piece buffer damages the caller"; exit 1; }
grep -q "control: old implementation damaged the caller in 0" "$LOG" && { echo "FAIL: the control did not fail, so the test cannot detect the defect"; exit 1; }
echo "PASS: every piece is exact and no caller field is touched; the control reproduces the defect" | tee -a "$LOG"
