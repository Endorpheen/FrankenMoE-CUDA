#!/usr/bin/env bash
# Full runtime over HTTP: prompt in, real generated tokens out, one Goose unit.
set -euo pipefail
HERE=$(cd "$(dirname "$0")" && pwd)
ROOT=$(cd "$HERE/../../.." && pwd)
GOOSE=/home/end0/AI/LlamaServer/toolchain/goose-2026.3-string/goose
SUP=/home/end0/AI/LlamaServer/apex-independent-fast-20261116/run
MODEL=${MODEL:-/home/end0/AI/LocalAI/Models/Apex-35B-V1.0-Q4_K_M.gguf}
OUT=${1:-/tmp/goose-http-completion}
mkdir -p "$OUT"

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
    "$ROOT/goose/model/qwen35-trunk.goose" \
    "$ROOT/goose/misc/gguf-headers.goose" "$ROOT/goose/model/gguf-tensors.goose" "$ROOT/goose/model/gguf-format.goose" \
    "$ROOT/goose/model/special-partition.goose" "$ROOT/goose/sampling/sampler-chain-full.goose" >/dev/null
( cd "$OUT" && "$GOOSE" --standalone -o unit.c --header unit.h unit.goose >/dev/null )

gcc-13 -O1 -I"$OUT" -I"$ROOT/vendor/flint/ggml/include" -I"$ROOT/vendor/flint/include" -o "$OUT/server" \
    "$ROOT/integration/goose-standalone/tests/generation_host.c" "$OUT/unit.c" \
    "$ROOT/experiments/EXP-2026-11-17-938-goose-http-transport/source/forwards.c" \
    "$ROOT/integration/goose-standalone/graph-bindings.c" \
    "$SUP/libggml-base.so.0" "$SUP/libggml-cpu.so.0" -Wl,-rpath,"$SUP" -lm

PORT=0
for p in 18931 18932 18933; do ss -H -ltn 2>/dev/null | grep -q ":$p " || { PORT=$p; break; }; done
[ "$PORT" != 0 ] || { echo "no free port"; exit 1; }

LD_LIBRARY_PATH="$SUP" "$OUT/server" "$MODEL" "$PORT" 4 > "$OUT/server.log" 2>&1 &
SRV=$!
for _ in $(seq 1 180); do grep -q 'SERVER listening' "$OUT/server.log" 2>/dev/null && break; sleep 1; done

P=$(curl -s --max-time 60 "http://127.0.0.1:$PORT/props")
C=$(curl -s --max-time 900 -X POST --data-binary 'The capital of France is' "http://127.0.0.1:$PORT/completion")
C2=$(curl -s --max-time 900 -X POST --data-binary 'The capital of France is' "http://127.0.0.1:$PORT/completion")
CHAT=$(curl -s --max-time 900 -X POST --data-binary $'<|im_start|>user\nWhat is 2+2?<|im_end|>\n<|im_start|>assistant\n' \
       "http://127.0.0.1:$PORT/completion")
kill $SRV 2>/dev/null || true
wait $SRV 2>/dev/null || true

echo "P: $P"
echo "C: $(echo "$C" | head -c 160)..."
echo "CHAT: $(echo "$CHAT" | head -c 160)..."
fail=0
echo "$P" | grep -q '"n_layer":40' || fail=1
echo "$C" | grep -q '"ids":\[11751,11,264,3177,8807,303,3712,321' || fail=1
echo "$C" | grep -q '"text":" Paris, a city rich in history' || fail=1
# A second request must reproduce the first: the per-request state is reset, so
# nothing from the previous request leaks into the next one.
[ "$C" = "$C2" ] || { echo "repeat differs"; fail=1; }
# The chat turn must stop on the Goose end-of-generation set, not at the cap.
echo "$CHAT" | grep -q ',248046\]' || { echo "chat did not stop on im_end"; fail=1; }
echo "$CHAT" | grep -q '<|im_end|>' || { echo "chat text missing the stop token"; fail=1; }
[ "$fail" = 0 ] && echo "HTTP_COMPLETION OK" || { echo "HTTP_COMPLETION FAIL"; tail -5 "$OUT/server.log"; exit 1; }
