#!/usr/bin/env bash
# Long prompt through the allocator-backed graph: a prompt the fixed-arena build
# could not serve at all.
set -euo pipefail
HERE=$(cd "$(dirname "$0")" && pwd)
OUT=${1:-/tmp/goose-http-completion}
mkdir -p "$OUT"
MODEL=${MODEL:-/home/end0/AI/LocalAI/Models/Apex-35B-V1.0-Q4_K_M.gguf}
SUP=/home/end0/AI/LlamaServer/apex-independent-fast-20261116/run

if [ ! -x "$OUT/server" ]; then
    ROOT=$(cd "$HERE/../../.." && pwd)
    GOOSE=/home/end0/AI/LlamaServer/toolchain/goose-2026.3-string/goose
    python3 "$ROOT/tools/compose-goose.py" "$OUT/unit.goose" \
        "$ROOT/goose/server/http-listener.goose" "$ROOT/goose/server/route-table.goose" \
        "$ROOT/goose/model/gguf-reader.goose" \
        "$ROOT/goose/misc/unicode-tables.goose" "$ROOT/goose/misc/unicode-text.goose" \
        "$ROOT/goose/misc/pretokenizer-qwen35.goose" \
        "$ROOT/goose/model/bpe-tokenize.goose" "$ROOT/goose/model/bpe-detokenize.goose" \
        "$ROOT/goose/model/tokenizer-engine.goose" \
        "$ROOT/goose/cpu/goose-graph-builders.goose" "$ROOT/goose/model/qgate-split.goose" \
        "$ROOT/goose/model/qwen35-attention.goose" "$ROOT/goose/model/gdn-ops.goose" \
        "$ROOT/goose/model/qwen35-gdn.goose" "$ROOT/goose/model/qwen35-kv.goose" \
        "$ROOT/goose/model/qwen35-trunk.goose" "$ROOT/goose/model/gguf-tensors.goose" \
        "$ROOT/goose/misc/gguf-headers.goose" "$ROOT/goose/model/gguf-tensors.goose" "$ROOT/goose/model/gguf-format.goose" \
        "$ROOT/goose/model/special-partition.goose" "$ROOT/goose/sampling/sampler-chain-full.goose" >/dev/null
    ( cd "$OUT" && "$GOOSE" --standalone -o unit.c --header unit.h unit.goose >/dev/null )
    gcc-13 -O1 -I"$OUT" -I"$ROOT/vendor/flint/ggml/include" -I"$ROOT/vendor/flint/include" -o "$OUT/server" \
        "$ROOT/integration/goose-standalone/tests/generation_host.c" "$OUT/unit.c" \
        "$ROOT/experiments/EXP-2026-11-17-938-goose-http-transport/source/forwards.c" \
        "$ROOT/integration/goose-standalone/graph-bindings.c" \
        "$SUP/libggml-base.so.0" "$SUP/libggml-cpu.so.0" -Wl,-rpath,"$SUP" -lm
fi

python3 - "$OUT/p517.txt" <<'PY'
import sys
para = ("The history of computing is a long story of abstraction, from relays and vacuum tubes to transistors. ") * 60
open(sys.argv[1], "w").write(para[:2400])
PY

PORT=0
for p in 19121 19122 19123; do ss -H -ltn 2>/dev/null | grep -q ":$p " || { PORT=$p; break; }; done
[ "$PORT" != 0 ] || { echo "no free port"; exit 1; }

LD_LIBRARY_PATH="$SUP" GOOSE_THREADS=8 "$OUT/server" "$MODEL" "$PORT" 1 > "$OUT/long.log" 2>&1 &
SRV=$!
for _ in $(seq 1 180); do grep -q 'SERVER listening' "$OUT/long.log" 2>/dev/null && break; sleep 1; done

R=$(curl -s --max-time 2400 -X POST --data-binary "@$OUT/p517.txt" "http://127.0.0.1:$PORT/completion")
kill $SRV 2>/dev/null || true
wait $SRV 2>/dev/null || true

echo "R: $(echo "$R" | head -c 120)..."
grep 'TIME' "$OUT/long.log" || true
fail=0
echo "$R" | grep -q '"count":[1-9]' || fail=1
grep -q 'prompt=517' "$OUT/long.log" || fail=1
grep -q 'not enough space' "$OUT/long.log" && fail=1
[ "$fail" = 0 ] && echo "HTTP_LONG_PROMPT OK" || { echo "HTTP_LONG_PROMPT FAIL"; tail -5 "$OUT/long.log"; exit 1; }
