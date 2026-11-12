#!/bin/bash
# usage: run.sh <tag> <signals>
set -u
TAG=$1; SIGS=$2
U=/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108/runtime/exp421/unified
LOG=/tmp/opencode/p801/$TAG.log
OUT=/tmp/opencode/p801/$TAG.result
cd "$U"
LD_LIBRARY_PATH=$U setsid ./llama-server \
  -m /home/end0/AI/LocalAI/Models/Apex-35B-V1.0-Q4_K_M.gguf --alias Apex-35B-V1.0 \
  -c 262144 -ngl all -fit off --cpu-moe -fa on -ctk q4_0 -ctv q4_0 \
  -b 4096 -ub 4096 -t 8 -tb 16 --cpu-mask 0f0f --cpu-mask-batch ffff \
  --cpu-strict 1 --cpu-strict-batch 1 --poll 0 -np 1 --cache-ram 0 \
  --ctx-checkpoints 32 --load-mode mmap --spec-type none --jinja \
  --temp 1 --top-p 0.95 --top-k 20 --min-p 0 \
  --host 127.0.0.1 --port 18093 --moe-cache-mib 3584 >"$LOG" 2>&1 &
PID=$!
t0=$(date +%s)
ready=0
for i in $(seq 1 600); do
  if ! kill -0 "$PID" 2>/dev/null; then break; fi
  code=$(curl -s -o /dev/null -w '%{http_code}' --max-time 2 http://127.0.0.1:18093/health || true)
  if [ "$code" = "200" ]; then ready=1; break; fi
  sleep 0.5
done
t1=$(date +%s)
if [ "$ready" != "1" ]; then
  echo "TAG=$TAG READY=0 LOAD_S=$((t1-t0))" > "$OUT"
  wait "$PID" 2>/dev/null; echo "EXIT=$?" >> "$OUT"
  exit 1
fi
health=$(curl -s --max-time 5 http://127.0.0.1:18093/health)
kill -INT "$PID" 2>/dev/null
if [ "$SIGS" = "2" ]; then sleep 0.2; kill -INT "$PID" 2>/dev/null; fi
t2=$(date +%s)
wait "$PID"; RC=$?
t3=$(date +%s)
{
  echo "TAG=$TAG READY=1 LOAD_S=$((t1-t0))"
  echo "SIGNALS=$SIGS"
  echo "HEALTH=$health"
  echo "EXIT=$RC"
  echo "SHUTDOWN_S=$((t3-t2))"
  echo "LEFT_PROC=$(pgrep -f 'llama-server .*18093' | wc -l)"
  echo "SECOND_MSG=$(grep -c 'second interrupt' "$LOG")"
  echo "CLEANUP=$(grep -c 'cleaning up before exit' "$LOG")"
} > "$OUT"
cat "$OUT"
