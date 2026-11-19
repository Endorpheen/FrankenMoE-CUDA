#!/usr/bin/env bash
# The two cases EXP-1398 left open: a request cut by n_predict in the middle of a multi-byte character, and a
# streaming request cancelled halfway. Neither may corrupt the answer or the server.
set -uo pipefail
R=$(cd "$(dirname "$0")/../.." && pwd)
BIN=${1:-$R/builds/flint-acc/bin}
OUT=${2:-/tmp/stream-edges}; rm -rf "$OUT"; mkdir -p "$OUT"
export LD_LIBRARY_PATH="$BIN"
M=/home/end0/AI/LocalAI/Models/Apex-35B-V1.0-Q4_K_M.gguf
setsid "$BIN/llama-server" -m "$M" --alias A -c 4096 -ngl all -fit off --cpu-moe \
  -fa on -ctk q4_0 -ctv q4_0 -b 2048 -ub 2048 -t 8 -tb 16 --poll 0 -np 1 --cache-ram 0 \
  --moe-cache-mib 4992 --temp 0 --seed 3 --host 127.0.0.1 --port 9101 > "$OUT/server.log" 2>&1 &
pid=$!
trap 'kill -TERM -$pid 2>/dev/null' EXIT
for i in $(seq 1 120); do curl -sf http://127.0.0.1:9101/health >/dev/null 2>&1 && break; sleep 4; done
fail=0

echo "--- truncation in the middle of multi-byte text"
for n in 2 3 5; do
  q='日本語で長い文章を書いてください。'
  python3 -c "import json,sys;print(json.dumps({'prompt':sys.argv[1],'n_predict':int(sys.argv[2]),'temperature':0,'seed':3}))" "$q" "$n" > "$OUT/b.json"
  curl -s -X POST -H 'Content-Type: application/json' --data @"$OUT/b.json" http://127.0.0.1:9101/completion > "$OUT/plain.json"
  python3 -c "import json,sys;print(json.dumps({'prompt':sys.argv[1],'n_predict':int(sys.argv[2]),'temperature':0,'seed':3,'stream':True}))" "$q" "$n" > "$OUT/b2.json"
  curl -s -N -X POST -H 'Content-Type: application/json' --data @"$OUT/b2.json" http://127.0.0.1:9101/completion > "$OUT/stream.txt"
  python3 - "$OUT/plain.json" "$OUT/stream.txt" "$n" <<'PY'
import json, sys
plain = json.load(open(sys.argv[1])).get("content", "")
parts = []
for line in open(sys.argv[2], encoding="utf-8", errors="replace"):
    line = line.strip()
    if not line.startswith("data: "): continue
    p = line[6:]
    if p == "[DONE]": break
    try: parts.append(json.loads(p).get("content", ""))
    except Exception: pass
streamed = "".join(parts)
same = streamed == plain
# the answer must be valid UTF-8 with no replacement characters, or truncation would have split a sequence
clean = "\ufffd" not in plain and "\ufffd" not in streamed
print(f"  n_predict={sys.argv[3]:>2}  {'equal' if same else 'DIFFERS'}  utf8-clean={clean}  len={len(plain)}")
raise SystemExit(0 if (same and clean) else 1)
PY
  [ $? -ne 0 ] && fail=1
done

echo "--- a streaming request cancelled halfway"
timeout 2 curl -s -N -X POST -H 'Content-Type: application/json' \
  --data '{"prompt":"Count slowly from one to five hundred.","n_predict":400,"temperature":0,"seed":3,"stream":true}' \
  http://127.0.0.1:9101/completion > "$OUT/cancel.txt" 2>/dev/null || true
echo "  cancelled after $(grep -c '^data: ' "$OUT/cancel.txt" 2>/dev/null || echo 0) fragments"
sleep 1
code=$(curl -s -o "$OUT/after.json" -w '%{http_code}' -X POST -H 'Content-Type: application/json' \
  --data '{"prompt":"Say ok.","n_predict":4,"temperature":0,"seed":3}' http://127.0.0.1:9101/completion)
body=$(python3 -c "import json;print(json.load(open('$OUT/after.json')).get('content','').strip()[:20])" 2>/dev/null || echo "")
echo "  after cancel: http=$code content='$body'"
[ "$code" = "200" ] || fail=1
kill -0 $pid 2>/dev/null || { echo "  server died after cancel"; fail=1; }
exit $fail
