#!/usr/bin/env bash
# Benchmark one runtime on the common Maxwell/Apex load (2048 prompt / 128 gen).
#
# Usage: bash tools/same-load-bench.sh <run-dir> <model.gguf> [ctx] [b] [ub] [repeats]
#
# Starts exactly one server on 9101, waits for health, runs the same token ids
# for every model, prints prefill/decode per run and medians, then stops the
# server and verifies the port is free.
set -euo pipefail
RUN=${1:?run dir}
MODEL=${2:?model}
CTX=${3:-4096}
B=${4:-2048}
UB=${5:-2048}
REPEATS=${6:-3}
PAYLOAD=${PAYLOAD:-/tmp/req2048.json}
PORT=9101
LOG=${BENCH_LOG:-/home/end0/AI/LlamaServer/maxwell-build-20261117/same-load-$(basename "$RUN").log}

[ -x "$RUN/llama-server" ] || { echo "no llama-server in $RUN"; exit 2; }
[ -f "$PAYLOAD" ] || { echo "missing payload $PAYLOAD"; exit 2; }
if [ -n "$(ss -H -ltn "sport = :$PORT")" ]; then
    echo "port $PORT occupied; refusing"; exit 2
fi

setsid env LD_LIBRARY_PATH="$RUN" "$RUN/llama-server" -m "$MODEL" \
    -c "$CTX" -ngl all --cpu-moe --moe-cache-mib 4992 -fa on \
    -b "$B" -ub "$UB" -t 8 -tb 16 -np 1 --host 127.0.0.1 --port "$PORT" > "$LOG" 2>&1 &
SRV=$!
cleanup() { kill "$SRV" 2>/dev/null || true; for _ in $(seq 1 30); do ss -H -ltn "sport = :$PORT" | grep -q . || break; sleep 1; done; }
trap cleanup EXIT

for _ in $(seq 1 300); do
    [ "$(curl -s -m 2 "http://127.0.0.1:$PORT/health" || true)" = '{"status":"ok"}' ] && break
    sleep 1
done
[ "$(curl -s -m 2 "http://127.0.0.1:$PORT/health" || true)" = '{"status":"ok"}' ] || {
    echo "server did not become healthy"; tail -20 "$LOG"; exit 4; }

echo "run=$RUN ctx=$CTX b=$B ub=$UB repeats=$REPEATS"
python3 - "$PORT" "$REPEATS" "$PAYLOAD" <<'PY'
import json, statistics, sys, urllib.request
port, repeats, payload = sys.argv[1], int(sys.argv[2]), sys.argv[3]
req = json.load(open(payload))
def call():
    r = urllib.request.Request(f"http://127.0.0.1:{port}/completion",
                               data=json.dumps(req).encode(),
                               headers={"Content-Type": "application/json"})
    with urllib.request.urlopen(r, timeout=1800) as f:
        return json.load(f)["timings"]
w = call()
print(f"  warmup: prompt_n={w['prompt_n']} prefill={w['prompt_per_second']:.2f} decode={w['predicted_per_second']:.2f} (not counted)")
rows = []
for i in range(repeats):
    t = call()
    rows.append(t)
    print(f"  run {i+1}: prompt_n={t['prompt_n']} prefill={t['prompt_per_second']:.2f} "
          f"predicted_n={t['predicted_n']} decode={t['predicted_per_second']:.2f}")
print(f"MEDIAN prefill={statistics.median(x['prompt_per_second'] for x in rows):.4f} "
      f"decode={statistics.median(x['predicted_per_second'] for x in rows):.4f}")
PY

ps -o rss= -p "$SRV" 2>/dev/null | awk '{printf "RSS MiB %.1f\n", $1/1024}'
nvidia-smi --query-gpu=memory.used --format=csv,noheader
