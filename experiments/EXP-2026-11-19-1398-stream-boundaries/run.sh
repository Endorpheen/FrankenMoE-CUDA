#!/usr/bin/env bash
# Streaming against non-streaming, byte for byte. The stream is where chunk boundaries can split a UTF-8 sequence
# or drop a piece, and the detokeniser that produces those pieces is served by the port.
set -uo pipefail
R=$(cd "$(dirname "$0")/../.." && pwd)
BIN=${1:-$R/builds/flint-acc/bin}
OUT=${2:-/tmp/stream-check}; rm -rf "$OUT"; mkdir -p "$OUT"
export LD_LIBRARY_PATH="$BIN"
M=/home/end0/AI/LocalAI/Models/Apex-35B-V1.0-Q4_K_M.gguf
setsid "$BIN/llama-server" -m "$M" --alias A -c 4096 -ngl all -fit off --cpu-moe \
  -fa on -ctk q4_0 -ctv q4_0 -b 2048 -ub 2048 -t 8 -tb 16 --poll 0 -np 1 --cache-ram 0 \
  --moe-cache-mib 4992 --temp 0 --seed 5 --host 127.0.0.1 --port 9101 > "$OUT/server.log" 2>&1 &
pid=$!
trap 'kill -TERM -$pid 2>/dev/null' EXIT
for i in $(seq 1 120); do curl -sf http://127.0.0.1:9101/health >/dev/null 2>&1 && break; sleep 4; done
fail=0
for q in "Say hello in Russian." "Write one line of Python." "Name two colours in Japanese."; do
  body_plain=$(python3 -c "import json,sys;print(json.dumps({'prompt':sys.argv[1],'n_predict':48,'temperature':0,'seed':5}))" "$q")
  body_stream=$(python3 -c "import json,sys;print(json.dumps({'prompt':sys.argv[1],'n_predict':48,'temperature':0,'seed':5,'stream':True}))" "$q")
  curl -s -X POST -H 'Content-Type: application/json' --data "$body_plain" http://127.0.0.1:9101/completion > "$OUT/plain.json"
  curl -s -N -X POST -H 'Content-Type: application/json' --data "$body_stream" http://127.0.0.1:9101/completion > "$OUT/stream.txt"
  python3 - "$OUT/plain.json" "$OUT/stream.txt" "$q" <<'PY'
import json, sys
plain = json.load(open(sys.argv[1])).get("content", "")
chunks = []
for line in open(sys.argv[2], encoding="utf-8", errors="replace"):
    line = line.strip()
    if not line.startswith("data: "):
        continue
    payload = line[6:]
    if payload == "[DONE]":
        break
    try:
        d = json.loads(payload)
    except Exception:
        continue
    chunks.append(d.get("content", ""))
streamed = "".join(chunks)
ok = streamed == plain
print(f"  {'equal  ' if ok else 'DIFFERS'}  chunks={len(chunks):3d} plain={len(plain):3d} chars  q={sys.argv[3][:28]!r}")
if not ok:
    for i, (a, b) in enumerate(zip(plain, streamed)):
        if a != b:
            print(f"    first difference at {i}: plain={a!r} streamed={b!r}")
            break
    else:
        print(f"    lengths differ: plain {len(plain)} streamed {len(streamed)}")
    raise SystemExit(1)
PY
  [ $? -ne 0 ] && fail=1
done
exit $fail
