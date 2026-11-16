#!/usr/bin/env bash
set -euo pipefail
ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
RUN="$ROOT/run"
exec 9>/tmp/apex-test.lock
flock -n 9 || { echo 'Another Apex lifecycle holds the lock'; exit 2; }
if [ -n "$(ss -H -ltn 'sport = :9101')" ]; then
    echo 'Port 9101 is occupied; no server was started'
    exit 2
fi
exec env LD_LIBRARY_PATH="$RUN" "$RUN/llama-server" \
    -m /home/end0/AI/LocalAI/Models/Apex-35B-V1.0-Q4_K_M.gguf \
    --alias Apex-35B-V1.0 \
    -c 262144 -ngl all -fit off --cpu-moe -fa on -ctk q4_0 -ctv q4_0 \
    -b 4096 -ub 4096 -t 8 -tb 16 \
    --cpu-mask 0f0f --cpu-mask-batch ffff --cpu-strict 1 --cpu-strict-batch 1 \
    --poll 0 -np 1 --cache-ram 0 --ctx-checkpoints 32 --load-mode mmap \
    --spec-type none --jinja --temp 1 --top-p 0.95 --top-k 20 --min-p 0 \
    --host 127.0.0.1 --port 9101 --moe-cache-mib 4992
