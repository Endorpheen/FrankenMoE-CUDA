#!/usr/bin/env bash
set -uo pipefail
R=/home/end0/AI/FrankenMoE-CUDA; BIN=$R/builds/flint-acc/bin; PORT=9101
OUT=$R/results/grammar-verify/$(date +%Y%m%d-%H%M%S); mkdir -p "$OUT"
exec 9>/tmp/apex-test.lock; flock -w 600 9 || exit 3
owner=$(ss -ltnp 2>/dev/null | grep ":$PORT" | head -1); [ -n "$owner" ] && { echo "port busy: $owner"; exit 4; }
GGML_GOOSE_GRAMMAR=1 GGML_GOOSE_GRAMMAR_VERIFY=1 LD_LIBRARY_PATH=$BIN "$BIN/llama-server" \
  -m /home/end0/AI/LocalAI/Models/Apex-35B-V1.0-Q4_K_M.gguf --alias Apex-35B-V1.0 \
  -c 262144 -ngl all -fit off --cpu-moe -fa on -ctk q4_0 -ctv q4_0 -b 4096 -ub 4096 -t 8 -tb 16 \
  --cpu-mask 0f0f --cpu-mask-batch ffff --cpu-strict 1 --cpu-strict-batch 1 --poll 0 -np 1 \
  --cache-ram 0 --ctx-checkpoints 32 --load-mode mmap --spec-type none --jinja --temp 0 \
  --moe-cache-mib 4992 --host 127.0.0.1 --port $PORT > "$OUT/server.log" 2>&1 &
PID=$!
for i in $(seq 1 120); do curl -sf "http://127.0.0.1:$PORT/health" >/dev/null 2>&1 && break; sleep 2; done
python3 - <<'PY'
import json, urllib.request
grammar = r'''
root   ::= "{" space "\"city\"" space ":" space string space "," space "\"country\"" space ":" space string space "}"
string ::= "\"" ( [a-zA-Z] )* "\""
space  ::= " "?
'''
body = json.dumps({"prompt": "Reply with a JSON object holding a city and its country.",
                   "n_predict": 24, "temperature": 0.0, "grammar": grammar}).encode()
req = urllib.request.Request("http://127.0.0.1:9101/completion", data=body, method="POST",
                             headers={"Content-Type": "application/json"})
with urllib.request.urlopen(req, timeout=900) as r:
    print("content:", json.load(r).get("content"))
PY
kill $PID 2>/dev/null; wait $PID 2>/dev/null
for i in $(seq 1 20); do ss -ltn 2>/dev/null | grep -q ":$PORT" || break; sleep 1; done
echo "--- verify markers:"; grep -a "FG171_GRAMMAR" "$OUT/server.log" | head -6
echo "artefacts: $OUT"
