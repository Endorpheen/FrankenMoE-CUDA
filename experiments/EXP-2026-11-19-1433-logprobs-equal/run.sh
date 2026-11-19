#!/usr/bin/env bash
# The same text request with top-5 logprobs, once with the native encoder and once with the ported one serving: the
# token ids and their probabilities must be identical, which is the piece of P11-02 the hashes alone do not show.
set -uo pipefail
R=$(cd "$(dirname "$0")/../.." && pwd)
BIN=${1:-$R/builds/flint-acc/bin}
OUT=${2:-/tmp/logprobs}; rm -rf "$OUT"; mkdir -p "$OUT"
export LD_LIBRARY_PATH="$BIN"
M=/home/end0/AI/LocalAI/Models/Apex-35B-V1.0-Q4_K_M.gguf
run() {
  label=$1; shift
  setsid "$BIN/llama-server" -m "$M" --alias A -c 4096 -ngl all -fit off --cpu-moe \
    -fa on -ctk q4_0 -ctv q4_0 -b 2048 -ub 2048 -t 8 -tb 16 --poll 0 -np 1 --cache-ram 0 \
    --moe-cache-mib 4992 --temp 0 --seed 21 --host 127.0.0.1 --port 9101 > "$OUT/$label.log" 2>&1 &
  local pid=$!
  for i in $(seq 1 120); do curl -sf http://127.0.0.1:9101/health >/dev/null 2>&1 && break; sleep 4; done
  curl -s -X POST -H 'Content-Type: application/json' --data '{
    "prompt":"List the first five prime numbers and explain why each is prime.",
    "n_predict":48,"temperature":0,"seed":21,"logprobs":true,"top_logprobs":5}' \
    http://127.0.0.1:9101/completion > "$OUT/$label.json"
  kill -TERM -$pid 2>/dev/null; wait $pid 2>/dev/null
}
FG_TK_BPE_DIAG=1 FG_TK_BPE_SERVE=0 run native
FG_TK_BPE_DIAG=1 run served
python3 - "$OUT" <<'PY'
import json, sys
d = sys.argv[1]
def load(name):
    j = json.load(open(f"{d}/{name}.json"))
    return j.get("content", ""), j.get("completion_probabilities", [])
c1, p1 = load("native"); c2, p2 = load("served")
print(f"content equal: {c1 == c2}   steps: {len(p1)} vs {len(p2)}")
same = 0; diff = 0
for a, b in zip(p1, p2):
    ta = a.get("top_logprobs", []); tb = b.get("top_logprobs", [])
    ok = a.get("tok_str") == b.get("tok_str") and len(ta) == len(tb) and all(
        x.get("tok_str") == y.get("tok_str") and abs(x.get("prob", 0) - y.get("prob", 0)) < 1e-9
        for x, y in zip(ta, tb))
    if ok: same += 1
    else:
        diff += 1
        if diff <= 2: print("  first difference at step", same + diff - 1, a.get("tok_str"), b.get("tok_str"))
print(f"steps with identical top-5 probabilities: {same}, differing: {diff}")
raise SystemExit(0 if (c1 == c2 and diff == 0 and same > 0) else 1)
PY
