#!/usr/bin/env bash
# One model load, 84 detokenise calls over three shapes, seven lengths and every flag pair.
set -uo pipefail
R=$(cd "$(dirname "$0")/../.." && pwd)
D=$(dirname "$0")
LIB=$R/builds/flint-acc/bin
gcc-13 -O2 -o /tmp/seq_harness "$D/harness.c" -I "$R/vendor/flint/include" -I "$R/vendor/flint/ggml/include" \
    -L "$LIB" -lllama -lggml-base -Wl,-rpath,"$LIB" || exit 1
out=$(FG_TK_DETOK_DIAG=1 /tmp/seq_harness 2>&1) || { echo "$out" | tail -3; exit 1; }
diffs=$(echo "$out" | grep -c "FG_TK_SEQ_DIFF" || true)
progress=$(echo "$out" | grep "FG_TK_SEQ_PROGRESS" | tail -1)
echo "$(echo "$out" | tail -1) | $progress | diffs=$diffs"
[ "$diffs" -eq 0 ]
