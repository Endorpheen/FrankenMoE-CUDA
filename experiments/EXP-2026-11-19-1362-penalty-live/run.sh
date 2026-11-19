#!/usr/bin/env bash
# The penalty door on a profile that actually uses it: a live counter, and the answer it produces.
set -uo pipefail
BIN=${1:-$PWD/builds/flint-acc/bin}
OUT=${2:-/tmp/pen}
mkdir -p "$OUT"
export LD_LIBRARY_PATH="$BIN"
MODEL=/home/end0/AI/LocalAI/Models/Apex-35B-V1.0-Q4_K_M.gguf
setsid "$BIN/llama-server" -m "$MODEL" --alias Apex-35B-V1.0 -c 4096 -ngl all -fit off --cpu-moe \
  -fa on -ctk q4_0 -ctv q4_0 -b 2048 -ub 2048 -t 8 -tb 16 --poll 0 -np 1 --cache-ram 0 \
  --moe-cache-mib 4992 --host 127.0.0.1 --port 9101 \
  --repeat-penalty 1.15 --repeat-last-n 256 --temp 0.8 --top-p 0.95 > "$OUT/server.log" 2>&1 &
PID=$!
trap 'kill -TERM -$PID 2>/dev/null' EXIT
for i in $(seq 1 140); do
  curl -sf http://127.0.0.1:9101/health >/dev/null 2>&1 && break
  kill -0 $PID 2>/dev/null || { echo "server died"; exit 1; }
  sleep 5
done
curl -sf http://127.0.0.1:9101/health >/dev/null || { echo "not ready"; exit 1; }
curl -s -X POST -H 'Content-Type: application/json' --data '{
  "messages":[{"role":"user","content":"Count from one to five in words."}],
  "max_tokens":48, "temperature":0.8
}' http://127.0.0.1:9101/v1/chat/completions > "$OUT/resp.json"
python3 -c "import json;d=json.load(open('$OUT/resp.json'));print('answer:',d['choices'][0]['message']['content'][:160].replace(chr(10),' '))" 2>/dev/null
echo "--- counters ---"
grep "FG171_PENALTIES" "$OUT/server.log" | tail -3; grep -c "FG171_PENALTIES" "$OUT/server.log"
