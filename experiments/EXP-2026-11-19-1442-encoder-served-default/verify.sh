#!/usr/bin/env bash
# The ported encoder on the shipped call path, serving by default (rule 10) and equal to the native answer.
# CPU only: the vocabulary is loaded, no weights and no inference.
set -uo pipefail
R=$(cd "$(dirname "$0")/../.." && pwd)
D=$R/experiments/EXP-2026-11-19-1442-encoder-served-default
BIN=$R/builds/flint-acc/bin
MODEL=${MODEL:-/home/end0/AI/LocalAI/Models/Apex-35B-V1.0-Q4_K_M.gguf}
P=$R/experiments/EXP-2026-11-19-1375-encoder-specials-fixed/prompts_wide.txt
[ -f "$P" ] || P=$R/experiments/EXP-2026-11-19-1375-encoder-specials-fixed/prompts.txt
OUT=$D/out; mkdir -p "$OUT"; LOG=$OUT/verify.log
{
    echo "# command: gcc tok_ids.c; run with FG_TK_BPE_SERVE=0 and by default, compare"
    gcc -O1 -o "$OUT/tok_ids" "$D/tok_ids.c" -I "$R/vendor/flint/include" -I "$R/vendor/flint/ggml/include" \
        -L "$BIN" -lllama -lggml-base -Wl,-rpath,"$BIN" || exit 1
    LD_LIBRARY_PATH=$BIN FG_TK_BPE_SERVE=0 "$OUT/tok_ids" "$MODEL" < "$P" > "$OUT/native.txt" 2>"$OUT/native.err"
    LD_LIBRARY_PATH=$BIN                          "$OUT/tok_ids" "$MODEL" < "$P" > "$OUT/served.txt" 2>"$OUT/served.err"
    LD_LIBRARY_PATH=$BIN FG_TK_BPE_DIAG=1         "$OUT/tok_ids" "$MODEL" < "$P" > /dev/null          2>"$OUT/diag.err"
    echo "--- comparison"
    if cmp -s "$OUT/native.txt" "$OUT/served.txt"; then
        echo "encoder: served answer equals native on $(wc -l < "$P") prompts"
    else
        echo "encoder: DIFFERS"; diff <(head -3 "$OUT/native.txt") <(head -3 "$OUT/served.txt") | head -6; exit 1
    fi
    echo "--- which implementation answered"
    grep -c FG_TK_BPE_SERVED "$OUT/diag.err" | sed 's/^/FG_TK_BPE_SERVED lines: /'
    grep -m2 -o "FG_TK_BPE_COMPARE calls=[0-9]* disagreements=[0-9]*" "$OUT/diag.err"
    grep -m2 -o "FG_TK_BPE_REFUSED.*" "$OUT/diag.err" || echo "FG_TK_BPE_REFUSED: none, the port answered every prompt"
} 2>&1 | tee "$LOG"
rc=${PIPESTATUS[0]}
[ "$rc" -eq 0 ] || { echo "FAIL"; exit 1; }
grep -q "FG_TK_BPE_SERVED" "$LOG" || { echo "FAIL: nothing was served, the flag gate is still in place"; exit 1; }
grep -q "disagreements=0" "$LOG" || { echo "FAIL: the comparison reports disagreements"; exit 1; }
echo "PASS: the ported encoder serves by default and equals the native answer"
