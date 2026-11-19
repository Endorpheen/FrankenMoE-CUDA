#!/usr/bin/env bash
# One lifecycle with ngram speculation enabled, purely to see whether the speculative prefix door is reached on
# real tokens. Functional check: the profile differs from the baseline by design.
set -uo pipefail
BIN=${1:-$PWD/builds/flint-acc/bin}
OUT=${2:-/tmp/spec-live}
mkdir -p "$OUT"
export LD_LIBRARY_PATH="$BIN"
export FG_ACCEPT_DIAG=1
MODEL=/home/end0/AI/LocalAI/Models/Apex-35B-V1.0-Q4_K_M.gguf
setsid "$BIN/llama-server" -m "$MODEL" --alias Apex-35B-V1.0 -c 8192 -ngl all -fit off --cpu-moe \
  -fa on -ctk q4_0 -ctv q4_0 -b 4096 -ub 4096 -t 8 -tb 16 \
  --cpu-mask 0f0f --cpu-mask-batch ffff --cpu-strict 1 --cpu-strict-batch 1 \
  --poll 0 -np 1 --cache-ram 0 --ctx-checkpoints 4 --load-mode mmap \
  --spec-type ngram-map-k,ngram-simple --jinja --temp 1 --top-p 0.95 --top-k 20 --min-p 0 \
  --moe-cache-mib 4992 --host 127.0.0.1 --port 9101 > "$OUT/server.log" 2>&1 &
PID=$!
trap 'kill -TERM -$PID 2>/dev/null' EXIT
for i in $(seq 1 140); do
  curl -sf http://127.0.0.1:9101/health >/dev/null 2>&1 && break
  kill -0 $PID 2>/dev/null || { echo "server died"; tail -6 "$OUT/server.log"; exit 1; }
  sleep 5
done
curl -sf http://127.0.0.1:9101/health >/dev/null || { echo "not ready"; exit 1; }
cat > "$OUT/req.json" <<'JSON'
{"prompt":"Continue the sequence exactly, repeating the same block over and over:\nA B C D A B C D A B C D A B C D A B C D A B C D","n_predict":128,"seed":123,"temperature":0.2}
JSON
curl -s http://127.0.0.1:9101/completion -H 'Content-Type: application/json' --data-binary @"$OUT/req.json" -o "$OUT/response.json"
echo "--- spec witnesses ---"
grep -oE "FG_PREFIX_ENTER[^\"]*" "$OUT/server.log" | head -4
grep -c "fg_sp_prefix\|FG_PREFIX" "$OUT/server.log"
echo "content chars: $(python3 -c "import json;print(len(json.load(open('$OUT/response.json')).get('content','')))")"
