#!/bin/sh
# Apex 32K/128 gate for the merged candidate. INERT until run.
# Goal: one request on the merged candidate, comparing content against the accepted baseline e68df05b,
#       optionally the same request on the native build for an A/B.
# Duration: about 20-60 s per run (load plus 32K prompt and 128 tokens).
# Runs: 1 by default; MODE=both adds the native arm (2 total).
set -u
ROOT=/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108
CAND=$ROOT/runtime/reconstructed/candidate-20261112
NATIVE=/home/end0/AI/LlamaServer/llama.cpp-flint-cuda/build-cuda/bin
REC=$ROOT/runtime/results/restore-exp150-32k128
OUT=$ROOT/runtime/reconstructed/apex-gate
mkdir -p "$OUT"

run_arm() {
    arm=$1; bindir=$2; port=$3
    echo "== $arm"
    env -i PATH=/usr/bin:/bin HOME=/tmp LD_LIBRARY_PATH="$bindir" \
        "$bindir/llama-server" -m /home/end0/AI/LocalAI/Models/Apex-35B-V1.0-Q4_K_M.gguf --alias Apex-35B-V1.0 \
        -c 262144 -ngl all -fit off --cpu-moe -fa on -ctk q4_0 -ctv q4_0 -b 4096 -ub 4096 -t 8 -tb 16 \
        --port "$port" > "$OUT/$arm.server.log" 2>&1 &
    pid=$!
    i=0
    while [ $i -lt 240 ]; do
        sleep 1
        curl -s --max-time 1 "http://127.0.0.1:$port/health" | grep -q '"ok"' && break
        i=$((i+1))
    done
    curl -s --max-time 600 -X POST "http://127.0.0.1:$port/completion" \
        -H 'Content-Type: application/json' --data-binary @"$REC/request.json" -o "$OUT/$arm.response.json"
    kill "$pid" 2>/dev/null
    wait "$pid" 2>/dev/null
    python3 - "$OUT/$arm.response.json" <<'PY'
import hashlib, json, sys
d = json.load(open(sys.argv[1]))
c = d.get("content", "")
print(" tokens=%s sha256=%s" % (d.get("tokens_predicted"), hashlib.sha256(c.encode()).hexdigest()[:40]))
PY
}

run_arm candidate "$CAND" 18301
if [ "${MODE:-candidate}" = both ]; then
    run_arm native "$NATIVE" 18302
fi
echo "accepted baseline content prefix: e68df05b4829865dcf8cd2424febfaded15b055f56"
