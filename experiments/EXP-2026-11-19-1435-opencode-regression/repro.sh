#!/usr/bin/env bash
# The scenario the acceptance triple cannot reach: a TEXT prompt through the chat endpoint, long enough to need a
# real prefill, streamed, exactly as a client like OpenCode sends it.
#
# Why it is separate: the acceptance request carries token ids, so it never exercises the tokenizer, the chat
# template, or the transition from a multi-thousand-token prefill into the first sampled token. The failure this
# reproduces - "get_sub_batch: failed to add token -2100184624 at index 0" - happened at exactly that transition,
# with FG171_TOP_P kept=1 and FG171_DIST sel=0 immediately before it.
#
# usage: repro.sh [binary-dir] [port]
set -uo pipefail
R=$(cd "$(dirname "$0")/../.." && pwd)
BIN=${1:-$R/builds/flint-acc/bin}
PORT=${2:-9101}
OUT=${3:-/tmp/opencode-repro}; rm -rf "$OUT"; mkdir -p "$OUT"
export LD_LIBRARY_PATH="$BIN"
M=/home/end0/AI/LocalAI/Models/Apex-35B-V1.0-Q4_K_M.gguf

setsid "$BIN/llama-server" -m "$M" --alias Apex-35B-V1.0 \
  -c 262144 -ngl all -fit off --cpu-moe -fa on -ctk q4_0 -ctv q4_0 \
  -b 4096 -ub 4096 -t 8 -tb 16 \
  --cpu-mask 0f0f --cpu-mask-batch ffff --cpu-strict 1 --cpu-strict-batch 1 \
  --poll 0 -np 1 --cache-ram 0 --ctx-checkpoints 32 --load-mode mmap \
  --spec-type none --jinja --temp 1 --top-p 0.95 --top-k 20 --min-p 0 \
  --moe-cache-mib 4992 --host 127.0.0.1 --port "$PORT" > "$OUT/server.log" 2>&1 &
PID=$!
trap 'kill -TERM -$PID 2>/dev/null' EXIT
for i in $(seq 1 180); do
  curl -sf "http://127.0.0.1:$PORT/health" >/dev/null 2>&1 && break
  kill -0 $PID 2>/dev/null || { echo "FAIL: server died during load"; tail -5 "$OUT/server.log"; exit 1; }
  sleep 4
done
curl -sf "http://127.0.0.1:$PORT/health" >/dev/null || { echo "FAIL: not ready"; exit 1; }

# a context-sized text prompt through the chat template, streamed: the shape a coding client sends
python3 - "$OUT" "$PORT" <<'PY'
import json, socket, sys, time
out, port = sys.argv[1], int(sys.argv[2])
body_text = ("You are a coding assistant. " + "The repository contains a runtime that serves a model. " * 1950 +
             "Now describe the architecture in one paragraph.")
body = json.dumps({"messages": [{"role": "user", "content": body_text}],
                   "max_tokens": 64, "temperature": 1, "top_p": 0.95, "top_k": 20, "stream": True}).encode()
req = (b"POST /v1/chat/completions HTTP/1.1\r\nHost: 127.0.0.1\r\nContent-Type: application/json\r\n"
       b"Connection: close\r\nContent-Length: " + str(len(body)).encode() + b"\r\n\r\n" + body)
s = socket.create_connection(("127.0.0.1", port), timeout=600)
s.sendall(req)
buf = b""
t0 = time.time()
while True:
    chunk = s.recv(65536)
    if not chunk:
        break
    buf += chunk
    if time.time() - t0 > 540:
        break
s.close()
text = buf.decode("utf-8", "replace")
open(f"{out}/stream.txt", "w").write(text)
ok = 'data: [DONE]' in text or '"finish_reason"' in text
print(f"stream bytes: {len(buf)}, saw completion marker: {ok}")
sys.exit(0 if ok else 1)
PY
STREAM_RC=$?

echo "--- the sequence the failure showed"
grep -E "FG171_TOP_P|FG171_DIST|get_sub_batch|failed to add token|Compute error" "$OUT/server.log" | tail -6
kill -0 $PID 2>/dev/null || { echo "FAIL: server died during the request"; exit 1; }
[ $STREAM_RC -eq 0 ] || { echo "FAIL: the stream did not complete"; exit 1; }
echo "PASS: the server survived a streamed chat request on a long text prompt"
