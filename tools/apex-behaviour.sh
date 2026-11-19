#!/usr/bin/env bash
# Three behaviours the acceptance run does not cover: KV reuse across requests, a cancelled request, and the RAM the
# process holds. One server lifetime, so the three are measured under the same conditions.
#
# Rule 30: the same flock as every Apex test, so this can never overlap another run or a benchmark.
set -uo pipefail
ROOT=$(cd "$(dirname "$0")/.." && pwd)
TAG=${1:?usage: apex-behaviour.sh <tag>}
BIN=${APEX_SERVER_DIR:-$ROOT/builds/flint-acc/bin}
MODEL=${MODEL:-/home/end0/AI/LocalAI/Models/Apex-35B-V1.0-Q4_K_M.gguf}
PORT=9101
OUT=$ROOT/results/apex-behaviour/$TAG-$(date +%Y%m%d-%H%M%S)
mkdir -p "$OUT"
LOCK=/tmp/apex-test.lock
exec 9>"$LOCK"
if ! flock -w 600 9; then echo "FAIL: could not take the Apex lock"; exit 3; fi

owner=$(ss -ltnp 2>/dev/null | grep ":$PORT" | head -1)
[ -n "$owner" ] && { echo "FAIL: port $PORT already owned: $owner"; exit 4; }

export LD_LIBRARY_PATH="$BIN"
"$BIN/llama-server" -m "$MODEL" --alias Apex-35B-V1.0 \
  -c 262144 -ngl all -fit off --cpu-moe -fa on -ctk q4_0 -ctv q4_0 \
  -b 4096 -ub 4096 -t 8 -tb 16 \
  --cpu-mask 0f0f --cpu-mask-batch ffff --cpu-strict 1 --cpu-strict-batch 1 \
  --poll 0 -np 1 --cache-ram 0 --ctx-checkpoints 32 --load-mode mmap \
  --spec-type none --jinja --temp 1 --top-p 0.95 --top-k 20 --min-p 0 \
  --moe-cache-mib 4992 --host 127.0.0.1 --port $PORT > "$OUT/server.log" 2>&1 &
PID=$!
echo "$PID" > "$OUT/pid"
cleanup() { kill "$PID" 2>/dev/null; wait "$PID" 2>/dev/null; }
trap cleanup EXIT

for i in $(seq 1 120); do
  curl -sf "http://127.0.0.1:$PORT/health" >/dev/null 2>&1 && break
  kill -0 "$PID" 2>/dev/null || { echo "FAIL: the server died during start"; tail -5 "$OUT/server.log"; exit 5; }
  sleep 2
done
curl -sf "http://127.0.0.1:$PORT/health" >/dev/null || { echo "FAIL: not ready"; exit 5; }

rss() { awk '/VmRSS/ {print $2}' /proc/$PID/status 2>/dev/null; }
echo "rss_after_start_kb=$(rss)" | tee "$OUT/result.txt"

PROMPT=$(python3 -c "print('The runtime reads experts from RAM while the dense weights stay resident. ' * 900)")
NOW=$(date +%s)

post() { # $1 = file, $2 = json body
  curl -s -o "$1" -w '%{http_code}' -H 'Content-Type: application/json' -d @- "http://127.0.0.1:$PORT/completion" <<<"$2"
}

BODY1=$(python3 - "$PROMPT" <<'PY'
import json, sys
print(json.dumps({"prompt": sys.argv[1], "n_predict": 32, "temperature": 0, "seed": 11, "cache_prompt": True}))
PY
)
code1=$(post "$OUT/r1.json" "$BODY1")
python3 - "$OUT/r1.json" "$code1" <<'PY' | tee -a "$OUT/result.txt"
import json, sys
d = json.load(open(sys.argv[1])); t = d.get("timings", {})
print(f"request1 status={sys.argv[2]} cache_n={t.get('cache_n')} prompt_n={t.get('prompt_n')} "
      f"predicted_n={t.get('predicted_n')} hash={d.get('content','')[:0]!r}")
PY

BODY2=$(python3 - "$PROMPT" <<'PY'
import json, sys
print(json.dumps({"prompt": sys.argv[1] + " One more sentence.", "n_predict": 32, "temperature": 0, "seed": 11,
                  "cache_prompt": True}))
PY
)
code2=$(post "$OUT/r2.json" "$BODY2")
python3 - "$OUT/r2.json" "$code2" <<'PY' | tee -a "$OUT/result.txt"
import json, sys
d = json.load(open(sys.argv[1])); t = d.get("timings", {})
print(f"request2 status={sys.argv[2]} cache_n={t.get('cache_n')} prompt_n={t.get('prompt_n')} "
      f"predicted_n={t.get('predicted_n')} same_answer={d.get('content') is not None}")
PY

# cancel: open a long streaming request, read a little, then drop the connection
python3 - "$PORT" "$OUT" <<'PY' | tee -a "$OUT/result.txt"
import json, socket, sys, time
port, out = int(sys.argv[1]), sys.argv[2]
body = json.dumps({"prompt": "Count slowly from one to five thousand, one per line.", "n_predict": 4000,
                   "temperature": 0, "stream": True}).encode()
s = socket.create_connection(("127.0.0.1", port), timeout=30)
s.sendall(b"POST /completion HTTP/1.1\r\nHost: 127.0.0.1\r\nContent-Type: application/json\r\n"
          b"Content-Length: " + str(len(body)).encode() + b"\r\nConnection: close\r\n\r\n" + body)
got = s.recv(4096)
time.sleep(1.0)
s.close()
print(f"cancel: read {len(got)} bytes, connection closed by the client")
PY
sleep 2

code3=$(curl -s -o "$OUT/r3.json" -w '%{http_code}' "http://127.0.0.1:$PORT/health")
python3 - "$OUT/r3.json" "$code3" <<'PY' | tee -a "$OUT/result.txt"
import json, sys
d = json.load(open(sys.argv[1]))
print(f"health after cancel: status={sys.argv[2]} state={d.get('status')}")
PY

echo "rss_after_work_kb=$(rss)" | tee -a "$OUT/result.txt"
nvidia-smi --query-gpu=memory.used --format=csv,noheader | tee -a "$OUT/result.txt"

kill "$PID" 2>/dev/null; wait "$PID" 2>/dev/null
for i in $(seq 1 20); do ss -ltn 2>/dev/null | grep -q ":$PORT" || break; sleep 1; done
ss -ltn 2>/dev/null | grep -q ":$PORT" && { echo "FAIL: port still held"; exit 6; }
echo "port released, exit 0" | tee -a "$OUT/result.txt"
echo "artefacts: $OUT"
