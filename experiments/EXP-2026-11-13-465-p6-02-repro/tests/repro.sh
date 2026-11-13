#!/bin/bash
set -u
U=/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108/runtime/exp421/unified
LOG=/tmp/opencode/p602/repro.log
OUT=/tmp/opencode/p602/repro.result
cd "$U"
LD_LIBRARY_PATH=$U setsid ./llama-server \
  -m /home/end0/AI/LocalAI/Models/Apex-35B-V1.0-Q4_K_M.gguf --alias Apex-35B-V1.0 \
  -c 2048 -ngl 0 -np 1 -t 8 --host 127.0.0.1 --port 18094 >"$LOG" 2>&1 &
PID=$!
ready=0
for i in $(seq 1 600); do
  kill -0 "$PID" 2>/dev/null || break
  c=$(curl -s -o /dev/null -w '%{http_code}' --max-time 2 http://127.0.0.1:18094/health || true)
  [ "$c" = "200" ] && { ready=1; break; }
  sleep 0.3
done
if [ "$ready" != "1" ]; then echo "READY=0"; kill -INT $PID 2>/dev/null; wait $PID 2>/dev/null; exit 1; fi
python3 - <<'PY' > /tmp/opencode/p602/body.json
import json
p = ("The quick brown fox jumps over the lazy dog. " * 40)
print(json.dumps({"prompt": p, "n_predict": 8, "temperature": 0.0, "cache_prompt": False}))
PY
resp=$(curl -s -m 300 -w '\nHTTP=%{http_code}' -H 'Content-Type: application/json' \
  --data-binary @/tmp/opencode/p602/body.json http://127.0.0.1:18094/completion || true)
alive=0; kill -0 "$PID" 2>/dev/null && alive=1
if [ "$alive" = "1" ]; then kill -INT "$PID" 2>/dev/null; wait "$PID" 2>/dev/null; rc=$?; else wait "$PID" 2>/dev/null; rc=$?; fi
{
  echo "READY=1"
  echo "ALIVE_AFTER_REQUEST=$alive"
  echo "SERVER_EXIT=$rc"
  echo "HTTP_TAIL=$(printf '%s' "$resp" | tail -1)"
  echo "CUDA_ERRORS=$(grep -c 'CUDA error' "$LOG")"
  echo "ABORT=$(grep -c 'ggml_abort\|terminate called' "$LOG")"
  echo "COPY=$(grep -c 'COPY src=' "$LOG")"
  echo "PROMPT_TOKENS=$(grep -o 'prompt eval time.*tokens' "$LOG" | tail -1)"
  echo "LEFT_PROC=$(pgrep -f 'llama-server .*18094' | wc -l)"
} | tee "$OUT"
