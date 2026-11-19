#!/usr/bin/env bash
# The ported encoder compared with the native one on the shipped call path, under a real server: every prompt
# the server tokenises is compared, including chat templates and specials.
set -uo pipefail
R=$(cd "$(dirname "$0")/../.." && pwd)
BIN=${1:-$R/builds/flint-acc/bin}
OUT=${2:-/tmp/enc-shadow}
rm -rf "$OUT"; mkdir -p "$OUT"
export LD_LIBRARY_PATH="$BIN"
export FG_TK_BPE_DIAG=1
MODEL=/home/end0/AI/LocalAI/Models/Apex-35B-V1.0-Q4_K_M.gguf
setsid "$BIN/llama-server" -m "$MODEL" --alias Apex-35B-V1.0 -c 8192 -ngl all -fit off --cpu-moe \
  -fa on -ctk q4_0 -ctv q4_0 -b 2048 -ub 2048 -t 8 -tb 16 --poll 0 -np 1 --cache-ram 0 \
  --moe-cache-mib 4992 --jinja --temp 0.7 --host 127.0.0.1 --port 9101 > "$OUT/server.log" 2>&1 &
PID=$!
trap 'kill -TERM -$PID 2>/dev/null' EXIT
for i in $(seq 1 120); do
  curl -sf http://127.0.0.1:9101/health >/dev/null 2>&1 && break
  kill -0 $PID 2>/dev/null || { echo "server died"; tail -3 "$OUT/server.log"; exit 1; }
  sleep 4
done
curl -sf http://127.0.0.1:9101/health >/dev/null || { echo "not ready"; exit 1; }
for q in "Hello" "Привет, мир" "日本語のテキスト" "def f(x): return x**2" "<|im_start|>system<|im_end|>"; do
  curl -s -X POST -H 'Content-Type: application/json' \
    --data "{\"messages\":[{\"role\":\"user\",\"content\":\"$q\"}],\"max_tokens\":8,\"temperature\":0}" \
    http://127.0.0.1:9101/v1/chat/completions > /dev/null
done
echo "--- counters ---"
grep "FG_TK_BPE_COMPARE" "$OUT/server.log" | tail -2
grep -c "FG_TK_BPE_DIFF" "$OUT/server.log"
