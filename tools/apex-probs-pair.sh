#!/usr/bin/env bash
# One prompt, two configurations: the shipped path and the same build with the Goose sampler doors switched off. The
# model distribution must be identical; the text may differ only where a threshold is crossed, and that is reported.
set -uo pipefail
ROOT=$(cd "$(dirname "$0")/.." && pwd)
OUT=$ROOT/results/prob-parity/$(date +%Y%m%d-%H%M%S)
mkdir -p "$OUT"
PORT=9101
BIN=${APEX_SERVER_DIR:-$ROOT/builds/flint-acc/bin}
MODEL=${MODEL:-/home/end0/AI/LocalAI/Models/Apex-35B-V1.0-Q4_K_M.gguf}
exec 9>/tmp/apex-test.lock
flock -w 600 9 || { echo "FAIL: no lock"; exit 3; }

run_once() { # $1 label, $2.. env
  local label=$1; shift
  local owner; owner=$(ss -ltnp 2>/dev/null | grep ":$PORT" | head -1)
  [ -n "$owner" ] && { echo "FAIL: port $PORT owned: $owner"; return 4; }
  env "$@" LD_LIBRARY_PATH="$BIN" "$BIN/llama-server" -m "$MODEL" --alias Apex-35B-V1.0 \
    -c 262144 -ngl all -fit off --cpu-moe -fa on -ctk q4_0 -ctv q4_0 -b 4096 -ub 4096 -t 8 -tb 16 \
    --cpu-mask 0f0f --cpu-mask-batch ffff --cpu-strict 1 --cpu-strict-batch 1 --poll 0 -np 1 \
    --cache-ram 0 --ctx-checkpoints 32 --load-mode mmap --spec-type none --jinja \
    --temp 0 --top-p 0.95 --top-k 20 --min-p 0 --moe-cache-mib 4992 \
    --host 127.0.0.1 --port $PORT > "$OUT/server-$label.log" 2>&1 &
  local pid=$!
  for i in $(seq 1 120); do
    curl -sf "http://127.0.0.1:$PORT/health" >/dev/null 2>&1 && break
    kill -0 "$pid" 2>/dev/null || { echo "FAIL: server died ($label)"; return 5; }
    sleep 2
  done
  python3 "$ROOT/tools/apex-probs.py" "$OUT" --label "$label" --base "http://127.0.0.1:$PORT"
  local rc=$?
  kill "$pid" 2>/dev/null; wait "$pid" 2>/dev/null
  for i in $(seq 1 20); do ss -ltn 2>/dev/null | grep -q ":$PORT" || break; sleep 1; done
  return $rc
}

run_once goose_on || exit $?
run_once doors_off GGML_GOOSE_SAMPLER_TOP_P=0 GGML_GOOSE_SAMPLER_PENALTIES=0 || exit $?

python3 - "$OUT" <<'PY'
import json, sys, glob
out = sys.argv[1]
a = json.load(open(f"{out}/goose_on.json")); b = json.load(open(f"{out}/doors_off.json"))
same = a["digest"] == b["digest"]
print(f"probability parity: goose_on={a['digest']} doors_off={b['digest']} identical={same} "
      f"positions={a['positions']}/{b['positions']}")
if not same:
    for i, (x, y) in enumerate(zip(a["first_positions"], b["first_positions"])):
        if x != y:
            print(f"  position {i}: goose_on={x[:3]} doors_off={y[:3]}")
            break
    raise SystemExit(1)
print("PASS: the model distribution is identical with the doors on and off")
PY
echo "artefacts: $OUT"
