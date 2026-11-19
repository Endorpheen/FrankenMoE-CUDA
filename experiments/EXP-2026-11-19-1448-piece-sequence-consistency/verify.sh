#!/usr/bin/env bash
# CPU only: vocabulary load, no weights, no inference.
set -uo pipefail
R=$(cd "$(dirname "$0")/../.." && pwd)
D=$R/experiments/EXP-2026-11-19-1448-piece-sequence-consistency
BIN=$R/builds/flint-acc/bin
MODEL=${MODEL:-/home/end0/AI/LocalAI/Models/Apex-35B-V1.0-Q4_K_M.gguf}
OUT=$D/out; mkdir -p "$OUT"; LOG=$OUT/verify.log
{
    echo "# command: gcc consistency.cpp; ./consistency $MODEL"
    gcc -O1 -o "$OUT/consistency" "$D/consistency.cpp" -I "$R/vendor/flint/include" -I "$R/vendor/flint/ggml/include" \
        -L "$BIN" -lllama -lggml-base -lstdc++ -Wl,-rpath,"$BIN" || exit 1
    LD_LIBRARY_PATH=$BIN "$OUT/consistency" "$MODEL"; rc=$?
    echo "exit=$rc"
    exit $rc
} 2>&1 | tee "$LOG"
rc=${PIPESTATUS[0]}
[ "$rc" -eq 0 ] || { echo "FAIL: the piece form and the sequence form disagree"; exit 1; }
grep -q "text_diffs=0 sequence_cases=400 sequence_diffs=0" "$LOG" || { echo "FAIL: the counts are not what passed"; exit 1; }
echo "PASS: streamed pieces join to the whole-sequence answer, and text survives a round trip" | tee -a "$LOG"
