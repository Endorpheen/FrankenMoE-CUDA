#!/bin/bash
set -u
S=/tmp/opencode/p602/newstage
LOG=/tmp/opencode/p602/fixed.log
cd $S
LD_LIBRARY_PATH=$S setsid ./llama-server -m /home/end0/AI/LocalAI/Models/Apex-35B-V1.0-Q4_K_M.gguf --alias Apex-35B-V1.0 \
  -c 2048 -ngl 0 -np 1 -t 8 --host 127.0.0.1 --port 18102 >"$LOG" 2>&1 &
PID=$!
ready=0
for i in $(seq 1 900); do kill -0 $PID 2>/dev/null || break; c=$(curl -s -o /dev/null -w '%{http_code}' --max-time 1 http://127.0.0.1:18102/health || true); [ "$c" = "200" ] && { ready=1; break; }; sleep 0.3; done
echo "READY=$ready"
python3 - <<'PY' > /tmp/opencode/p602/body.json
import json
print(json.dumps({"prompt":"The quick brown fox jumps over the lazy dog. "*40,"n_predict":8,"temperature":0.0,"cache_prompt":False}))
PY
code=$(curl -s -m 240 -o /tmp/opencode/p602/fixed.json -w '%{http_code}' -H 'Content-Type: application/json' --data-binary @/tmp/opencode/p602/body.json http://127.0.0.1:18102/completion || true)
alive=0; kill -0 $PID 2>/dev/null && alive=1
kill -INT $PID 2>/dev/null; wait $PID 2>/dev/null; rc=$?
echo "HTTP=$code ALIVE=$alive EXIT=$rc CUDA=$(grep -c 'CUDA error' "$LOG") ABORT=$(grep -c 'ggml_abort\|terminate called' "$LOG") LEFT=$(pgrep -f 'llama-server .*18102' | wc -l)"
python3 - <<'PY'
import json
d=json.load(open('/tmp/opencode/p602/fixed.json'))
c=d.get('content','')
print('CONTENT_OK' if c.startswith('The quick brown fox') or len(c)>0 else 'CONTENT_EMPTY', repr(c[:40]))
PY
pkill -f 'llama-server .*18102' 2>/dev/null || true
