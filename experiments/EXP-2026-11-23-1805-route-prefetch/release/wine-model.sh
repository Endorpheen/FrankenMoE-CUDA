#!/usr/bin/env bash
# CPU-only start of the Windows server under Wine with the real Apex GGUF:
# Wine has no nvcuda.dll, so this checks model mapping, the CPU path, the
# HTTP API and a short generation, not GPU inference.
set -uo pipefail
out=/home/end0/AI/claude-apex-100/release/windows
run="$out/wine-model"; mkdir -p "$run"
model=/home/end0/AI/LocalAI/Models/Apex-35B-V1.0-Q4_K_M.gguf
export WINEPREFIX="$out/wineprefix" WINEARCH=win64 WINEDEBUG=-all
export FRANKENMOE_WEB_ROOT; FRANKENMOE_WEB_ROOT=$(winepath -w "$out/web")
if ss -H -ltn 'sport = :9101' | grep -q .; then echo "port 9101 busy"; exit 2; fi
cd "$run"
setsid wine "$out/frankenmoe-server.exe" --ctx-size 4096 --batch-size 512 --ubatch-size 512 \
  --cache-type-k q4_0 --cache-type-v q4_0 --threads 8 --threads-batch 16 \
  "$(winepath -w "$model")" --serve 9101 > "$run/server.log" 2>&1 &
pid=$!; echo "$pid" > "$run/pid.txt"
ok=0
for i in $(seq 1 600); do
  kill -0 "$pid" 2>/dev/null || { echo "server exited"; break; }
  if curl -fsS --max-time 2 http://127.0.0.1:9101/health > "$run/health.json" 2>/dev/null; then ok=1; echo "health after ${i}s"; break; fi
  sleep 1
done
if [ $ok = 1 ]; then
  curl -fsS --max-time 2 http://127.0.0.1:9101/v1/models > "$run/models.json"; echo "models exit $?"
  curl -sS --max-time 900 -H 'Content-Type: application/json' -o "$run/chat.json" -w 'chat http=%{http_code} %{time_total}s\n' \
    --data '{"model":"frankenmoe","messages":[{"role":"user","content":"Say hello in one word."}],"max_tokens":8,"temperature":0}' \
    http://127.0.0.1:9101/v1/chat/completions
fi
kill -TERM -- -"$pid" 2>/dev/null || kill -TERM "$pid" 2>/dev/null
for i in $(seq 1 30); do kill -0 "$pid" 2>/dev/null || break; sleep 1; done
ss -H -ltn 'sport = :9101' | grep -q . && echo "WARNING port held" || echo "port released"
/usr/bin/tail -n 25 "$run/server.log"
