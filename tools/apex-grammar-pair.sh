#!/usr/bin/env bash
# A constrained request on the shipped build, with the ported grammar automaton on and off, compared byte for byte.
#
# The flag was switched off because an earlier version accepted a token the native stack rejected, which corrupted a byte.
# TD-S10 later proved the decisions equal over 1.7 million candidates; this asks the question on the shipped path, where a
# difference would show as different text rather than as a count.
set -uo pipefail
ROOT=$(cd "$(dirname "$0")/.." && pwd)
OUT=$ROOT/results/grammar-parity/$(date +%Y%m%d-%H%M%S)
mkdir -p "$OUT"
PORT=9101
BIN=${APEX_SERVER_DIR:-$ROOT/builds/flint-acc/bin}
MODEL=${MODEL:-/home/end0/AI/LocalAI/Models/Apex-35B-V1.0-Q4_K_M.gguf}
exec 9>/tmp/apex-test.lock
flock -w 600 9 || { echo "FAIL: no lock"; exit 3; }

run_once() { # $1 label, $2 value for GGML_GOOSE_GRAMMAR (empty = unset)
  local label=$1 gval=$2
  local owner; owner=$(ss -ltnp 2>/dev/null | grep ":$PORT" | head -1)
  [ -n "$owner" ] && { echo "FAIL: port $PORT owned: $owner"; return 4; }
  if [ -n "$gval" ]; then
    env GGML_GOOSE_GRAMMAR="$gval" LD_LIBRARY_PATH="$BIN" "$BIN/llama-server" -m "$MODEL" --alias Apex-35B-V1.0 \
      -c 262144 -ngl all -fit off --cpu-moe -fa on -ctk q4_0 -ctv q4_0 -b 4096 -ub 4096 -t 8 -tb 16 \
      --cpu-mask 0f0f --cpu-mask-batch ffff --cpu-strict 1 --cpu-strict-batch 1 --poll 0 -np 1 \
      --cache-ram 0 --ctx-checkpoints 32 --load-mode mmap --spec-type none --jinja --temp 0 \
      --moe-cache-mib 4992 --host 127.0.0.1 --port $PORT > "$OUT/server-$label.log" 2>&1 &
  else
    LD_LIBRARY_PATH="$BIN" "$BIN/llama-server" -m "$MODEL" --alias Apex-35B-V1.0 \
      -c 262144 -ngl all -fit off --cpu-moe -fa on -ctk q4_0 -ctv q4_0 -b 4096 -ub 4096 -t 8 -tb 16 \
      --cpu-mask 0f0f --cpu-mask-batch ffff --cpu-strict 1 --cpu-strict-batch 1 --poll 0 -np 1 \
      --cache-ram 0 --ctx-checkpoints 32 --load-mode mmap --spec-type none --jinja --temp 0 \
      --moe-cache-mib 4992 --host 127.0.0.1 --port $PORT > "$OUT/server-$label.log" 2>&1 &
  fi
  local pid=$!
  for i in $(seq 1 120); do
    curl -sf "http://127.0.0.1:$PORT/health" >/dev/null 2>&1 && break
    kill -0 "$pid" 2>/dev/null || { echo "FAIL: server died ($label)"; return 5; }
    sleep 2
  done
  python3 - "$OUT" "$label" <<'PY'
import json, sys, urllib.request
out, label = sys.argv[1], sys.argv[2]
# a JSON array of two fields: the shape a tool call needs, so the grammar has real work to do
grammar = r'''
root   ::= "{" space "\"city\"" space ":" space string space "," space "\"country\"" space ":" space string space "}"
string ::= "\"" ( [a-zA-Z] )* "\""
space  ::= " "?
'''
body = json.dumps({"prompt": "Reply with a JSON object holding a city and its country.",
                   "n_predict": 48, "temperature": 0.0, "grammar": grammar}).encode()
req = urllib.request.Request(f"http://127.0.0.1:9101/completion", data=body, method="POST",
                             headers={"Content-Type": "application/json"})
with urllib.request.urlopen(req, timeout=900) as r:
    d = json.load(r)
content = d.get("content", "")
open(f"{out}/{label}.txt", "w").write(content)
print(f"{label}: len={len(content)} content={content!r}")
PY
  local rc=$?
  kill "$pid" 2>/dev/null; wait "$pid" 2>/dev/null
  for i in $(seq 1 20); do ss -ltn 2>/dev/null | grep -q ":$PORT" || break; sleep 1; done
  return $rc
}

run_once grammar_off "" || exit $?
run_once grammar_on 1 || exit $?

if cmp -s "$OUT/grammar_off.txt" "$OUT/grammar_on.txt"; then
  echo "PASS: the constrained output is identical with the ported automaton on and off"
else
  echo "FAIL: the ported automaton changed the constrained output"
  diff "$OUT/grammar_off.txt" "$OUT/grammar_on.txt" | head -4
  exit 1
fi
grep -cE "FG_GRAMMAR" "$OUT/server-grammar_on.log" | sed 's/^/grammar door lines: /'
echo "artefacts: $OUT"
