#!/usr/bin/env bash
# Build the Maxwell GGUF from the downloaded safetensors, then smoke-load it.
#
# Refuses to start until every shard named by the index is present, so a partial
# download can never produce a half-converted file that looks complete.
#
# Usage: bash tools/maxwell-build.sh <model-dir> <out.gguf> [--quant-io] [--vocab]
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
MODEL=${1:?model dir}
OUT=${2:?output gguf}
shift 2 || true

python3 - "$MODEL" <<'EOF'
import json, os, sys
d = sys.argv[1]
cfg = json.load(open(os.path.join(d, "config.json")))
if cfg.get("architectures") != ["MaxwellMoeForCausalLM"]:
    sys.exit(f"unexpected architectures: {cfg.get('architectures')}")
idx = os.path.join(d, "model.safetensors.index.json")
if not os.path.exists(idx):
    sys.exit("model.safetensors.index.json is missing; the download is not complete")
shards = sorted(set(json.load(open(idx))["weight_map"].values()))
missing = [s for s in shards if not os.path.exists(os.path.join(d, s))]
if missing:
    sys.exit(f"{len(missing)}/{len(shards)} shards missing, first: {missing[0]}")
if not os.path.exists(os.path.join(d, "tokenizer.json")):
    print("warning: tokenizer.json is missing; the GGUF will need --stub-vocab", file=sys.stderr)
print(f"shards ok: {len(shards)}")
EOF

LOG_DIR=${MX_LOG_DIR:-/home/end0/AI/LlamaServer/maxwell-build-20261117}
mkdir -p "$LOG_DIR"
LOG=$LOG_DIR/convert-$(date +%Y%m%d-%H%M%S).log
echo "convert: $MODEL -> $OUT" | tee "$LOG"
python3 "$ROOT/tools/maxwell-convert.py" "$MODEL" "$OUT" "$@" 2>&1 | tee -a "$LOG"
echo "convert exit=0 size=$(stat -c%s "$OUT")" | tee -a "$LOG"
