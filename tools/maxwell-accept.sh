#!/usr/bin/env bash
# Acceptance run for the Maxwell GGUF.
#
# Steps: completeness check -> convert (unless the GGUF exists) -> one forward ->
# a short greedy generation. Nothing here starts a server or touches port 9101.
#
# Usage: bash tools/maxwell-accept.sh <model-dir> <out.gguf> [--run]
# Without --run it prints the plan and exits after the completeness check.
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
MODEL=${1:?model dir}
OUT=${2:?output gguf}
RUN=0
[ "${3:-}" = "--run" ] && RUN=1
BIN=${MX_BIN:-/home/end0/AI/LlamaServer/llama.cpp-flint-cuda/build-cuda/bin}
LOAD=${MX_LOAD:-/home/end0/AI/LlamaServer/maxwell-build-20261117/load_forward}
LOG_DIR=${MX_LOG_DIR:-/home/end0/AI/LlamaServer/maxwell-build-20261117}
mkdir -p "$LOG_DIR"

python3 - "$MODEL" <<'PY'
import json, os, sys
d = sys.argv[1]
idx = os.path.join(d, "model.safetensors.index.json")
if not os.path.exists(idx):
    sys.exit("download incomplete: model.safetensors.index.json missing")
shards = sorted(set(json.load(open(idx))["weight_map"].values()))
missing = [s for s in shards if not os.path.exists(os.path.join(d, s))]
if missing:
    sys.exit(f"download incomplete: {len(missing)}/{len(shards)} shards missing")
print(f"model complete: {len(shards)} shards")
PY

if [ ! -f "$OUT" ]; then
    echo "convert -> $OUT"
    [ "$RUN" = "1" ] || { echo "pass --run to convert"; exit 0; }
    python3 "$ROOT/tools/maxwell-convert.py" "$MODEL" "$OUT" | tee "$LOG_DIR/accept-convert.log"
else
    echo "GGUF exists: $(stat -c%s "$OUT") bytes"
fi

[ "$RUN" = "1" ] || { echo "planned: forward + generation (pass --run)"; exit 0; }

echo "== one forward ==" | tee "$LOG_DIR/accept.log"
"$LOAD" "$OUT" 2>&1 | tee -a "$LOG_DIR/accept.log"

echo "== greedy generation (32 tokens, ctx 4096) ==" | tee -a "$LOG_DIR/accept.log"
/usr/bin/time -v "$BIN/llama-cli" -m "$OUT" -c 4096 -n 32 --temp 0 \
    -p "The capital of France is" 2>&1 | tee -a "$LOG_DIR/accept.log"

nvidia-smi --query-gpu=memory.used --format=csv,noheader | tee -a "$LOG_DIR/accept.log"
echo "acceptance run finished: $LOG_DIR/accept.log"
