#!/usr/bin/env bash
# End-to-end gate: HTTP request -> Goose tokenizer -> ids in the response.
# The whole runtime is one composed Goose unit: listener, router, GGUF reader,
# vocabulary layout and tokenizer. No libllama is linked.
set -euo pipefail
HERE=$(cd "$(dirname "$0")" && pwd)
ROOT=$(cd "$HERE/../../.." && pwd)
GOOSE=/home/end0/AI/LlamaServer/toolchain/goose-2026.3-string/goose
SUP=/home/end0/AI/LlamaServer/apex-independent-fast-20261116/run
MODEL=${MODEL:-/home/end0/AI/LocalAI/Models/Apex-35B-V1.0-Q4_K_M.gguf}
OUT=${1:-/tmp/goose-http-tokenize}
mkdir -p "$OUT"

python3 "$ROOT/tools/compose-goose.py" "$OUT/unit.goose" \
    "$ROOT/goose/server/http-listener.goose" "$ROOT/goose/server/route-table.goose" \
    "$ROOT/goose/model/gguf-reader.goose" \
    "$ROOT/goose/misc/unicode-tables.goose" "$ROOT/goose/misc/unicode-text.goose" \
    "$ROOT/goose/misc/pretokenizer-qwen35.goose" \
    "$ROOT/goose/model/bpe-tokenize.goose" "$ROOT/goose/model/bpe-detokenize.goose" \
    "$ROOT/goose/model/tokenizer-engine.goose" >/dev/null
( cd "$OUT" && "$GOOSE" --standalone -o unit.c --header unit.h unit.goose >/dev/null )

gcc-13 -O1 -I"$OUT" -I"$ROOT/vendor/flint/ggml/include" -o "$OUT/server" \
    "$ROOT/integration/goose-standalone/tests/http_tokenize.c" "$OUT/unit.c" \
    "$ROOT/experiments/EXP-2026-11-17-938-goose-http-transport/source/forwards.c" \
    "$ROOT/integration/goose-standalone/graph-bindings.c" \
    "$SUP/libggml-base.so.0" "$SUP/libggml-cpu.so.0" -Wl,-rpath,"$SUP" -lm

PORT=0
for p in 18871 18872 18873 18874; do
    ss -H -ltn 2>/dev/null | grep -q ":$p " || { PORT=$p; break; }
done
[ "$PORT" != 0 ] || { echo "no free port"; exit 1; }

LD_LIBRARY_PATH="$SUP" "$OUT/server" "$MODEL" "$PORT" 6 > "$OUT/server.log" 2>&1 &
SRV=$!
for _ in $(seq 1 120); do grep -q 'SERVER listening' "$OUT/server.log" 2>/dev/null && break; sleep 1; done

A=$(curl -s --max-time 30 -X POST --data-binary 'The capital of France is' "http://127.0.0.1:$PORT/tokenize")
B=$(curl -s --max-time 30 -X POST --data-binary 'Hello, world!' "http://127.0.0.1:$PORT/tokenize")
H=$(curl -s --max-time 30 "http://127.0.0.1:$PORT/health")
D1=$(curl -s --max-time 30 -X POST --data-binary '{"ids":[760,6511,314,9338,369]}' "http://127.0.0.1:$PORT/detokenize")
D2=$(curl -s --max-time 30 -X POST --data-binary '{"ids":[9419,11,1814,0]}' "http://127.0.0.1:$PORT/detokenize")
P=$(curl -s --max-time 30 "http://127.0.0.1:$PORT/props")
kill $SRV 2>/dev/null || true
wait $SRV 2>/dev/null || true

echo "A: $A"
echo "B: $B"
echo "H: $H"
echo "D1: $D1"
echo "D2: $D2"
echo "P:  $P"
fail=0
echo "$A" | grep -q '\[760,6511,314,9338,369\]' || fail=1
echo "$B" | grep -q '\[9419,11,1814,0\]'            || fail=1
echo "$H" | grep -q '"route":1'                     || fail=1
echo "$D1" | grep -q '"text":"The capital of France is"' || fail=1
echo "$D2" | grep -q '"text":"Hello, world!"'            || fail=1
echo "$P"  | grep -q '"n_layer":41'                      || fail=1
echo "$P"  | grep -q '"n_embd":2048'                     || fail=1
[ "$fail" = 0 ] && echo "HTTP_TOKENIZE OK" || { echo "HTTP_TOKENIZE FAIL"; tail -5 "$OUT/server.log"; exit 1; }
