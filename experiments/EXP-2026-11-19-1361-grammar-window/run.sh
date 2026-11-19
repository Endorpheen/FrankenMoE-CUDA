#!/usr/bin/env bash
# A constrained request against the real vocabulary, with the grammar hook's counters on: how many pieces
# still fill the codepoint window at 64, and how many candidates the port declines.
set -uo pipefail
BIN=${1:-$PWD/builds/flint-acc/bin}
OUT=${2:-/tmp/grm}
mkdir -p "$OUT"
export LD_LIBRARY_PATH="$BIN"
export GGML_GOOSE_GRAMMAR_DIAG=1
MODEL=/home/end0/AI/LocalAI/Models/Apex-35B-V1.0-Q4_K_M.gguf
setsid "$BIN/llama-server" -m "$MODEL" --alias Apex-35B-V1.0 -c 4096 -ngl all -fit off --cpu-moe \
  -fa on -ctk q4_0 -ctv q4_0 -b 2048 -ub 2048 -t 8 -tb 16 --poll 0 -np 1 --cache-ram 0 \
  --moe-cache-mib 4992 --host 127.0.0.1 --port 9101 > "$OUT/server.log" 2>&1 &
PID=$!
trap 'kill -TERM -$PID 2>/dev/null' EXIT
for i in $(seq 1 140); do
  curl -sf http://127.0.0.1:9101/health >/dev/null 2>&1 && break
  kill -0 $PID 2>/dev/null || { echo "server died"; exit 1; }
  sleep 5
done
curl -sf http://127.0.0.1:9101/health >/dev/null || { echo "not ready"; exit 1; }
curl -s -X POST -H 'Content-Type: application/json' --data '{
  "messages":[{"role":"user","content":"Give one JSON object with two keys a and b, numbers."}],
  "response_format":{"type":"json_object"}, "max_tokens":64, "temperature":0.0
}' http://127.0.0.1:9101/v1/chat/completions > "$OUT/resp.json"
python3 -c "import json;d=json.load(open('$OUT/resp.json'));print('answer:',d['choices'][0]['message']['content'][:120])" 2>/dev/null
echo "--- counters ---"
grep -c "FG171_PREFILTER" "$OUT/server.log"
grep -o "FG171_TRUNC seen=[0-9]*" "$OUT/server.log" | tail -1
grep -c "FG171_GRAMMAR_SIZES" "$OUT/server.log"
