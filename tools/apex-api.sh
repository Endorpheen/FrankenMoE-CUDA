#!/usr/bin/env bash
# The API corpus on one server lifetime, under the same lock as every Apex test (rule 30).
set -uo pipefail
ROOT=$(cd "$(dirname "$0")/.." && pwd)
OUT=$ROOT/results/apex-api/$(date +%Y%m%d-%H%M%S)
mkdir -p "$OUT"
PORT=9101
BIN=${APEX_SERVER_DIR:-$ROOT/builds/flint-acc/bin}
MODEL=${MODEL:-/home/end0/AI/LocalAI/Models/Apex-35B-V1.0-Q4_K_M.gguf}
exec 9>/tmp/apex-test.lock
flock -w 600 9 || { echo "FAIL: no lock"; exit 3; }
owner=$(ss -ltnp 2>/dev/null | grep ":$PORT" | head -1)
[ -n "$owner" ] && { echo "FAIL: port $PORT owned: $owner"; exit 4; }

LD_LIBRARY_PATH="$BIN" "$BIN/llama-server" -m "$MODEL" --alias Apex-35B-V1.0 \
  -c 262144 -ngl all -fit off --cpu-moe -fa on -ctk q4_0 -ctv q4_0 -b 4096 -ub 4096 -t 8 -tb 16 \
  --cpu-mask 0f0f --cpu-mask-batch ffff --cpu-strict 1 --cpu-strict-batch 1 --poll 0 -np 1 \
  --cache-ram 0 --ctx-checkpoints 32 --load-mode mmap --spec-type none --jinja \
  --temp 1 --top-p 0.95 --top-k 20 --min-p 0 --moe-cache-mib 4992 \
  --host 127.0.0.1 --port $PORT > "$OUT/server.log" 2>&1 &
PID=$!
trap 'kill $PID 2>/dev/null; wait $PID 2>/dev/null' EXIT
for i in $(seq 1 120); do
  curl -sf "http://127.0.0.1:$PORT/health" >/dev/null 2>&1 && break
  kill -0 "$PID" 2>/dev/null || { echo "FAIL: server died during start"; tail -5 "$OUT/server.log"; exit 5; }
  sleep 2
done
python3 "$ROOT/tools/apex-api.py" "http://127.0.0.1:$PORT" | tee "$OUT/corpus.txt"
RC=${PIPESTATUS[0]}
kill "$PID" 2>/dev/null; wait "$PID" 2>/dev/null
for i in $(seq 1 20); do ss -ltn 2>/dev/null | grep -q ":$PORT" || break; sleep 1; done
ss -ltn 2>/dev/null | grep -q ":$PORT" && { echo "FAIL: port still held"; exit 6; }
echo "port released, exit $RC"; echo "artefacts: $OUT"
exit $RC
