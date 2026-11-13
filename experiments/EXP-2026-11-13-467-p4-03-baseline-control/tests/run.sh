#!/bin/bash
set -u
TAG=$1; NP=$2
F=/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108
BIN=$F/runtime/reconstructed/build/stage-run
LOG=/tmp/opencode/p403/$TAG.log
cd "$BIN"
python3 - "$NP" > /tmp/opencode/p403/$TAG.req.json <<'PY'
import json,sys
d=json.load(open('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108/runtime/exp244/results-apex-or244-or-goose/request.json'))
d['n_predict']=int(sys.argv[1])
print(json.dumps(d))
PY
load_before=$(awk '{print $1" "$2" "$3}' /proc/loadavg)
builds_before=$(pgrep -fc 'clang|cc1|g\+\+|gcc' || true)
LD_LIBRARY_PATH=$BIN setsid ./llama-server \
  -m /home/end0/AI/LocalAI/Models/Apex-35B-V1.0-Q4_K_M.gguf --alias Apex-35B-V1.0 \
  -c 262144 -ngl all -fit off --cpu-moe -fa on -ctk q4_0 -ctv q4_0 \
  -b 4096 -ub 4096 -t 8 -tb 16 --cpu-mask 0f0f --cpu-mask-batch ffff \
  --cpu-strict 1 --cpu-strict-batch 1 --poll 0 -np 1 --cache-ram 0 \
  --ctx-checkpoints 32 --load-mode mmap --spec-type none --jinja \
  --temp 1 --top-p 0.95 --top-k 20 --min-p 0 \
  --host 127.0.0.1 --port 18093 --moe-cache-mib 3584 >"$LOG" 2>&1 &
PID=$!
ready=0
for i in $(seq 1 900); do
  kill -0 "$PID" 2>/dev/null || break
  c=$(curl -s -o /dev/null -w '%{http_code}' --max-time 1 http://127.0.0.1:18093/health || true)
  [ "$c" = "200" ] && { ready=1; break; }
  sleep 0.5
done
if [ "$ready" != "1" ]; then echo "$TAG READY=0"; kill -INT $PID 2>/dev/null; wait $PID 2>/dev/null; exit 1; fi
load_mid=$(awk '{print $1" "$2" "$3}' /proc/loadavg)
resp=$(curl -s -m 600 -H 'Content-Type: application/json' --data-binary @/tmp/opencode/p403/$TAG.req.json http://127.0.0.1:18093/completion)
printf '%s' "$resp" > /tmp/opencode/p403/$TAG.resp.json
kill -INT "$PID" 2>/dev/null; wait "$PID" 2>/dev/null; rc=$?
python3 - "$TAG" "$NP" "$load_before" "$load_mid" "$builds_before" "$rc" <<'PY'
import json,sys,hashlib,re
tag,np,lb,lm,bb,rc=sys.argv[1:]
d=json.load(open(f'/tmp/opencode/p403/{tag}.resp.json'))
tim=d.get('timings',{})
text=d.get('content','')
sha=hashlib.sha256(text.encode()).hexdigest()
# prefill/decode from timings
pre=tim.get('prompt_per_second')
dec=tim.get('predicted_per_second')
out={'tag':tag,'n_predict':int(np),'prefill_tok_s':pre,'decode_tok_s':dec,
     'prompt_tokens':tim.get('prompt_n'),'predicted':tim.get('predicted_n'),
     'content_sha256':sha,'server_exit':int(rc),
     'loadavg_before':lb,'loadavg_at_request':lm,'cpu_threads':__import__('os').cpu_count(),
     'parallel_compilers_before':int(bb)}
json.dump(out,open(f'/tmp/opencode/p403/{tag}.result.json','w'),indent=2)
print(json.dumps(out))
PY
pkill -f 'llama-server .*18093' 2>/dev/null || true
