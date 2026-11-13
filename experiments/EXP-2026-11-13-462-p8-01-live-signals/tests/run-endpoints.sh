#!/bin/bash
set -u
U=/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108/runtime/exp421/unified
LOG=/tmp/opencode/p801/p801-endpoints.log
OUT=/tmp/opencode/p801/p801-endpoints.result
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
seen503=0
for i in $(seq 1 600); do
  kill -0 "$PID" 2>/dev/null || break
  c=$(curl -s -o /dev/null -w '%{http_code}' --max-time 2 http://127.0.0.1:18093/health || true)
  if [ "$c" = "503" ]; then seen503=1; fi
  if [ "$c" = "200" ]; then break; fi
  sleep 0.2
done
health=$(curl -s --max-time 5 http://127.0.0.1:18093/health)
props=$(curl -s --max-time 10 http://127.0.0.1:18093/props)
metrics=$(curl -s -o /dev/null -w '%{http_code}' --max-time 10 http://127.0.0.1:18093/metrics)
pcount=$(printf '%s' "$props" | python3 -c 'import json,sys;d=json.load(sys.stdin);print(len(d))' 2>/dev/null || echo ERR)
phas=$(printf '%s' "$props" | python3 -c 'import json,sys;d=json.load(sys.stdin);print("default_generation_settings" in d, "total_slots" in d, "is_sleeping" in d, "cors_proxy_enabled" in d)' 2>/dev/null || echo ERR)
kill -INT "$PID" 2>/dev/null
wait "$PID"; RC=$?
{
  echo "SEEN_503=$seen503"
  echo "HEALTH=$health"
  echo "PROPS_KEYS=$pcount"
  echo "PROPS_HAS=$phas"
  echo "METRICS_HTTP=$metrics"
  echo "EXIT=$RC"
  echo "LEFT_PROC=$(pgrep -f 'llama-server .*18093' | wc -l)"
} > "$OUT"
cat "$OUT"
