#!/usr/bin/env bash
# A client that connects, sends a streaming request and closes without reading the body. The server must keep
# serving afterwards - the slot and the detokeniser state must not be left wedged (EXP-1399 left this open).
set -uo pipefail
R=$(cd "$(dirname "$0")/../.." && pwd)
BIN=${1:-$R/builds/flint-acc/bin}
OUT=${2:-/tmp/abandon}; rm -rf "$OUT"; mkdir -p "$OUT"
export LD_LIBRARY_PATH="$BIN"
M=/home/end0/AI/LocalAI/Models/Apex-35B-V1.0-Q4_K_M.gguf
setsid "$BIN/llama-server" -m "$M" --alias A -c 4096 -ngl all -fit off --cpu-moe \
  -fa on -ctk q4_0 -ctv q4_0 -b 2048 -ub 2048 -t 8 -tb 16 --poll 0 -np 1 --cache-ram 0 \
  --moe-cache-mib 4992 --temp 0 --seed 3 --host 127.0.0.1 --port 9101 > "$OUT/server.log" 2>&1 &
pid=$!
trap 'kill -TERM -$pid 2>/dev/null' EXIT
for i in $(seq 1 120); do curl -sf http://127.0.0.1:9101/health >/dev/null 2>&1 && break; sleep 4; done

# send a streaming request and close the socket without reading a single byte
python3 - <<'PY'
import socket, json
body = json.dumps({"prompt": "Count from one to five hundred slowly.", "n_predict": 400,
                   "temperature": 0, "seed": 3, "stream": True}).encode()
req = (b"POST /completion HTTP/1.1\r\nHost: 127.0.0.1:9101\r\nContent-Type: application/json\r\n"
       b"Content-Length: " + str(len(body)).encode() + b"\r\n\r\n" + body)
s = socket.create_connection(("127.0.0.1", 9101), timeout=5)
s.sendall(req)
s.close()          # no read at all
print("client closed without reading")
PY

sleep 3
code=$(curl -s -o "$OUT/after.json" -w '%{http_code}' -X POST -H 'Content-Type: application/json' \
  --data '{"prompt":"Say ok.","n_predict":4,"temperature":0,"seed":3}' http://127.0.0.1:9101/completion)
content=$(python3 -c "import json;print(json.load(open('$OUT/after.json')).get('content','').strip()[:20])" 2>/dev/null || echo "")
echo "after abandon: http=$code content='$content'"
kill -0 $pid 2>/dev/null && echo "server alive" || { echo "SERVER DIED"; exit 1; }
[ "$code" = "200" ] || exit 1
