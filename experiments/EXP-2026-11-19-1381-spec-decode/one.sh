#!/usr/bin/env bash
# One speculative configuration, one request, the numbers the server itself reports.
# usage: one.sh <label> [extra server args...]
set -uo pipefail
R=$(cd "$(dirname "$0")/../.." && pwd)
BIN=${BIN:-$R/builds/flint-acc/bin}
LABEL=$1; shift
OUT=/tmp/spec-$LABEL; rm -rf "$OUT"; mkdir -p "$OUT"
export LD_LIBRARY_PATH="$BIN"
MODEL=/home/end0/AI/LocalAI/Models/Apex-35B-V1.0-Q4_K_M.gguf
setsid "$BIN/llama-server" -m "$MODEL" --alias Apex-35B-V1.0 -c 4096 -ngl all -fit off --cpu-moe \
  -fa on -ctk q4_0 -ctv q4_0 -b 2048 -ub 2048 -t 8 -tb 16 --poll 0 -np 1 \
  --moe-cache-mib 4992 --host 127.0.0.1 --port 9101 --temp 0 --seed 1234 "$@" > "$OUT/server.log" 2>&1 &
PID=$!
trap 'kill -TERM -$PID 2>/dev/null' EXIT
for i in $(seq 1 120); do
  curl -sf http://127.0.0.1:9101/health >/dev/null 2>&1 && break
  kill -0 $PID 2>/dev/null || { echo "$LABEL: server died"; tail -3 "$OUT/server.log"; exit 1; }
  sleep 4
done
curl -sf http://127.0.0.1:9101/health >/dev/null || { echo "$LABEL: not ready"; exit 1; }
VRAM=$(nvidia-smi --query-gpu=memory.used --format=csv,noheader,nounits)
RSS=$(awk '/VmRSS/{print $2}' /proc/$PID/status 2>/dev/null)
curl -s -X POST -H 'Content-Type: application/json' --data '{
  "messages":[{"role":"user","content":"Write a detailed paragraph about the history of the printing press."}],
  "max_tokens":512, "temperature":0.0, "seed":1234
}' http://127.0.0.1:9101/v1/chat/completions > "$OUT/resp.json"
python3 - "$OUT/resp.json" "$LABEL" "$VRAM" "$RSS" <<'PY'
import json, sys
d = json.load(open(sys.argv[1]))
t = d.get("timings", {})
print(f"{sys.argv[2]:22s} decode {t.get('predicted_per_second',0):6.2f} tok/s  "
      f"prefill {t.get('prompt_per_second',0):7.1f} tok/s  "
      f"gen {t.get('predicted_n',0):4d}  vram {sys.argv[3]} MiB  rss {sys.argv[4]} kB")
PY
grep -oE "n_draft[^,]*|n_accept[^,]*|accept rate[^%]*%" "$OUT/server.log" | tail -3
