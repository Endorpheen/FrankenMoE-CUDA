#!/usr/bin/env bash
# One lifecycle with a non-default repeat penalty, purely to see whether the penalty door is reached on real
# tokens. This is a functional check, not a performance run: the profile differs from the baseline by design and
# nothing measured here is compared with it.
set -uo pipefail
BIN=${1:-$PWD/builds/flint-acc/bin}
OUT=${2:-/tmp/penalty-live}
# the same request with the penalty door switched off is the control; everything else is identical
DOOR_OFF=${PEN_DOOR_OFF:-0}
mkdir -p "$OUT"
export LD_LIBRARY_PATH="$BIN"
if [ "$DOOR_OFF" = "1" ]; then export GGML_GOOSE_SAMPLER_PENALTIES=0; fi
MODEL=/home/end0/AI/LocalAI/Models/Apex-35B-V1.0-Q4_K_M.gguf
setsid "$BIN/llama-server" -m "$MODEL" --alias Apex-35B-V1.0 -c 262144 -ngl all -fit off --cpu-moe \
  -fa on -ctk q4_0 -ctv q4_0 -b 4096 -ub 4096 -t 8 -tb 16 \
  --cpu-mask 0f0f --cpu-mask-batch ffff --cpu-strict 1 --cpu-strict-batch 1 \
  --poll 0 -np 1 --cache-ram 0 --ctx-checkpoints 32 --load-mode mmap --spec-type none --jinja \
  --temp 1 --top-p 0.95 --top-k 20 --min-p 0 --repeat-penalty 1.15 --repeat-last-n 64 \
  --moe-cache-mib 4992 --host 127.0.0.1 --port 9101 > "$OUT/server.log" 2>&1 &
PID=$!
trap 'kill -TERM -$PID 2>/dev/null' EXIT
for i in $(seq 1 120); do
  curl -sf http://127.0.0.1:9101/health >/dev/null 2>&1 && break
  kill -0 $PID 2>/dev/null || { echo "server died"; tail -5 "$OUT/server.log"; exit 1; }
  sleep 5
done
curl -sf http://127.0.0.1:9101/health >/dev/null || { echo "not ready"; exit 1; }
# A text prompt long enough that the penalty actually has repeat history to act on. The token-array prompt
# from the baseline request ends in 9 characters, which proves reachability and nothing else.
cat > "$OUT/request-64.json" <<'JSON'
{"prompt":"Write a paragraph about cats. Repeat the word cat often.","n_predict":96,"seed":123,"temperature":1.0}
JSON
curl -s http://127.0.0.1:9101/completion -H 'Content-Type: application/json' \
  -d @"$OUT/request-64.json" \
  -o "$OUT/response.json"
grep -oE "FG171_[A-Z_]+ goose calls=[0-9]+" "$OUT/server.log" | sort -u | tail -6
echo "response chars: $(wc -c < "$OUT/response.json")"
