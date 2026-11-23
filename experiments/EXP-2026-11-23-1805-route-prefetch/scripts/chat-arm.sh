#!/usr/bin/env bash
# One chat-server arm: fresh server (no GOOSE_* env), warm-up chat, measured chat, clean stop.
# usage: chat-arm.sh <tag> <server-binary> <web-dir>
set -u
D=/home/end0/.codex/worktrees/pure-goose-inkling-mini/FrankenMoE-CUDA/build/exp1786-split-planes
TAG="$1"; BINX="$2"; WEB="$3"
RUN="/home/end0/AI/claude-apex-100/runs/$TAG-$(date +%Y%m%d-%H%M%S)"; mkdir -p "$RUN"; echo "RUN=$RUN"
exec 9>/tmp/apex-test.lock; flock 9
if ss -H -ltn 'sport = :9101' | grep -q .; then echo "port 9101 busy"; exit 2; fi
mapfile -t args < "$D/argv.txt"
sha256sum "$BINX" > "$RUN/binary.sha256"
env | cut -d= -f1 | grep -E '^(GOOSE_|GGML_)' > "$RUN/inherited-env.txt" || true
cat /proc/vmstat > "$RUN/vmstat-start.txt"
setsid env FRANKENMOE_WEB_ROOT="$WEB" "$BINX" "${args[@]}" > "$RUN/server.log" 2>&1 &
pid=$!; echo "$pid" > "$RUN/pid.txt"
for i in $(seq 1 240); do
  kill -0 "$pid" 2>/dev/null || { echo "server died"; tail -20 "$RUN/server.log"; exit 3; }
  curl -fsS --max-time 2 http://127.0.0.1:9101/health > "$RUN/health.json" 2>/dev/null && break; sleep 1; done
echo "health after ${i}s"
nvidia-smi --query-compute-apps=pid,used_gpu_memory --format=csv,noheader,nounits --loop-ms=250 > "$RUN/vram.csv" & vp=$!
REQ='{"model":"frankenmoe","messages":[{"role":"user","content":"Write a short story about a lighthouse keeper who discovers a message in a bottle. Use about 300 words."}],"max_tokens":384,"temperature":0,"stream":false}'
for n in 1 2; do
  curl -sS --max-time 300 -H 'Content-Type: application/json' --data "$REQ" -w '%{http_code} %{time_total}\n' -o "$RUN/chat$n.json" http://127.0.0.1:9101/v1/chat/completions > "$RUN/client-$n.txt"
done
cat /proc/$pid/status > "$RUN/status-after.txt"; cat /proc/vmstat > "$RUN/vmstat-after.txt"
kill -TERM "$vp"; wait "$vp" 2>/dev/null
kill -TERM "$pid"; for i in $(seq 1 60); do kill -0 "$pid" 2>/dev/null || break; sleep 0.5; done
ss -H -ltn 'sport = :9101' | grep -q . && echo "WARNING port held" || echo "port released"
echo "$RUN" > /home/end0/AI/claude-apex-100/runs/LAST
