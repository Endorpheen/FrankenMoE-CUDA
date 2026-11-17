#!/usr/bin/env bash
# Compare the accepted Apex reference with a candidate runtime on the exact
# 32768/512 profile, one server at a time, on port 9101.
#
# Usage: bash tools/apex-compare.sh <reference|/path/to/candidate/run> [repeats]
#
# Refuses: if the reference manifest does not verify, if port 9101 has an owner,
# if one of our servers is still alive, or if the GPU has unrelated owners that
# leave too little VRAM (reported with numbers, nothing is stopped).
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
RUN=${1:?reference or candidate run directory}
REPEATS=${2:-3}
MODEL=${APEX_MODEL:-/home/end0/AI/LocalAI/Models/Apex-35B-V1.0-Q4_K_M.gguf}
PORT=9101
LOG=${APEX_LOG:-/home/end0/AI/LlamaServer/maxwell-build-20261117/apex-compare.log}

if [ "$RUN" = "reference" ]; then
    RUN=/home/end0/AI/LlamaServer/apex-independent-fast-20261116/run
    bash "$ROOT/tools/verify-apex-reference.sh" "$RUN"
fi
[ -x "$RUN/llama-server" ] || { echo "no llama-server in $RUN"; exit 2; }
if [ -n "$(ss -H -ltn "sport = :$PORT")" ]; then
    echo "port $PORT is occupied; not starting anything"; exit 2
fi
if pgrep -af 'llama-server' | grep -q 'FrankenMoE\|maxwell\|apex'; then
    echo "one of our servers is still alive; refusing"; exit 2
fi

USED=$(nvidia-smi --query-gpu=memory.used --format=csv,noheader,nounits | head -1)
TOTAL=$(nvidia-smi --query-gpu=memory.total --format=csv,noheader,nounits | head -1)
FREE=$((TOTAL - USED))
echo "VRAM used=$USED MiB total=$TOTAL MiB free=$FREE MiB"
if [ "$FREE" -lt 11500 ]; then
    echo "not enough free VRAM for the accepted profile (needs ~11.5 GiB)"
    nvidia-smi --query-compute-apps=pid,used_memory --format=csv,noheader
    echo "nothing was stopped; free the card or use a labelled reduced profile"
    exit 3
fi

mkdir -p "$(dirname "$LOG")"
: > "$LOG"
setsid env LD_LIBRARY_PATH="$RUN" "$RUN/llama-server" \
    -m "$MODEL" --alias Apex-35B-V1.0 \
    -c 262144 -ngl all -fit off --cpu-moe -fa on -ctk q4_0 -ctv q4_0 \
    -b 4096 -ub 4096 -t 8 -tb 16 \
    --cpu-mask 0f0f --cpu-mask-batch ffff --cpu-strict 1 --cpu-strict-batch 1 \
    --poll 0 -np 1 --cache-ram 0 --ctx-checkpoints 32 --load-mode mmap \
    --spec-type none --jinja --temp 1 --top-p 0.95 --top-k 20 --min-p 0 \
    --host 127.0.0.1 --port "$PORT" --moe-cache-mib 4992 >> "$LOG" 2>&1 &
SERVER=$!
cleanup() { kill "$SERVER" 2>/dev/null || true; for _ in $(seq 1 30); do ss -H -ltn "sport = :$PORT" | grep -q . || break; sleep 1; done; }
trap cleanup EXIT

for _ in $(seq 1 120); do
    [ "$(curl -s -m 2 "http://127.0.0.1:$PORT/health" || true)" = '{"status":"ok"}' ] && break
    sleep 1
done
if [ "$(curl -s -m 2 "http://127.0.0.1:$PORT/health" || true)" != '{"status":"ok"}' ]; then
    echo "server did not become healthy; see $LOG"; tail -20 "$LOG"; exit 4
fi

python3 - "$PORT" "$REPEATS" <<'PY'
import json, statistics, sys, urllib.request
port, repeats = sys.argv[1], int(sys.argv[2])
prompt = ("The history of computing spans several decades of invention and engineering. " * 1600)
rows = []
for i in range(repeats):
    body = {"prompt": prompt, "n_predict": 512, "cache_prompt": False}
    req = urllib.request.Request(f"http://127.0.0.1:{port}/completion",
                                 data=json.dumps(body).encode(),
                                 headers={"Content-Type": "application/json"})
    with urllib.request.urlopen(req, timeout=1800) as r:
        t = json.load(r)["timings"]
    rows.append(t)
    print(f"run {i+1}: prompt_n={t['prompt_n']} prefill={t['prompt_per_second']:.2f} decode={t['predicted_per_second']:.2f}")
print(f"MEDIAN prefill={statistics.median(x['prompt_per_second'] for x in rows):.4f} "
      f"decode={statistics.median(x['predicted_per_second'] for x in rows):.4f}")
PY

ps -o rss= -p "$SERVER" 2>/dev/null | awk '{printf "RSS MiB %.1f\n", $1/1024}'
nvidia-smi --query-gpu=memory.used --format=csv,noheader
