#!/usr/bin/env bash
# A 32K-token prompt takes seconds to prefill; closing the socket while it runs is the case EXP-1410 did not cover.
set -uo pipefail
R=$(cd "$(dirname "$0")/../.." && pwd)
BIN=${1:-$R/builds/flint-acc/bin}
OUT=${2:-/tmp/prefill-abandon}; rm -rf "$OUT"; mkdir -p "$OUT"
export LD_LIBRARY_PATH="$BIN"
M=/home/end0/AI/LocalAI/Models/Apex-35B-V1.0-Q4_K_M.gguf
setsid "$BIN/llama-server" -m "$M" --alias A -c 40960 -ngl all -fit off --cpu-moe \
  -fa on -ctk q4_0 -ctv q4_0 -b 4096 -ub 4096 -t 8 -tb 16 --poll 0 -np 1 --cache-ram 0 \
  --moe-cache-mib 4992 --temp 0 --seed 3 --host 127.0.0.1 --port 9101 > "$OUT/server.log" 2>&1 &
pid=$!
trap 'kill -TERM -$pid 2>/dev/null' EXIT
for i in $(seq 1 120); do curl -sf http://127.0.0.1:9101/health >/dev/null 2>&1 && break; sleep 4; done

python3 - <<'PY'
import socket, json, time
para = ("The history of the printing press begins with movable type in fifteenth-century Europe. ")
body = json.dumps({"prompt": (para * 1400)[:120000], "n_predict": 64, "temperature": 0, "seed": 3}).encode()
req = (b"POST /completion HTTP/1.1\r\nHost: 127.0.0.1:9101\r\nContent-Type: application/json\r\n"
       b"Content-Length: " + str(len(body)).encode() + b"\r\n\r\n" + body)
s = socket.create_connection(("127.0.0.1", 9101), timeout=5)
s.sendall(req)
time.sleep(0.8)      # prefill of ~32K tokens takes seconds, so this lands inside it
s.close()
print("client closed 0.8 s after sending a 32K-token prompt")
PY

sleep 5
code=$(curl -s -o "$OUT/after.json" -w '%{http_code}' -X POST -H 'Content-Type: application/json' \
  --data '{"prompt":"Say ok.","n_predict":4,"temperature":0,"seed":3}' http://127.0.0.1:9101/completion)
content=$(python3 -c "import json;print(json.load(open('$OUT/after.json')).get('content','').strip()[:20])" 2>/dev/null || echo "")
echo "after prefill abandon: http=$code content='$content'"
kill -0 $pid 2>/dev/null && echo "server alive" || { echo "SERVER DIED"; exit 1; }
[ "$code" = "200" ] || exit 1
