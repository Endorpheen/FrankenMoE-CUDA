#!/usr/bin/env bash
# Paired ON/OFF measurement: the configurations alternate (ON, OFF, ON, OFF, ...)
# so a slow drift of the machine cancels instead of being attributed to the stages.
set -uo pipefail
R=/home/end0/AI/FrankenMoE-CUDA
S=/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108/runtime/reconstructed/build/stage-run/llama-server
M=/home/end0/AI/LocalAI/Models/Apex-35B-V1.0-Q4_K_M.gguf
OUT=${1:-$R/results/p610-paired}
PAIRS=${2:-3}
mkdir -p "$OUT"; PORT=9101
OFF="GGML_GOOSE_SAMPLER_TYPICAL=0 GGML_GOOSE_SAMPLER_XTC=0 GGML_GOOSE_SAMPLER_PENALTIES=0 GGML_GOOSE_SAMPLER_TOP_P=0 GGML_GOOSE_SAMPLER_DRY=0"
ON="GGML_GOOSE_SAMPLER_TYPICAL=1 GGML_GOOSE_SAMPLER_XTC=1 GGML_GOOSE_SAMPLER_PENALTIES=1 GGML_GOOSE_SAMPLER_TOP_P=1 GGML_GOOSE_SAMPLER_DRY=1"

one() {
  local tag=$1; shift
  local log="$OUT/$tag.log"
  env LD_LIBRARY_PATH="$R/builds/flint-acc/bin" "$@" \
      "$S" -m "$M" --alias Apex-35B-V1.0 -c 262144 -ngl all -fit off --cpu-moe \
      -fa on -ctk q4_0 -ctv q4_0 -b 4096 -ub 4096 -t 8 -tb 16 \
      --cpu-mask 0f0f --cpu-mask-batch ffff --cpu-strict 1 --cpu-strict-batch 1 \
      --poll 0 -np 1 --cache-ram 0 --ctx-checkpoints 32 --load-mode mmap \
      --spec-type none --jinja --temp 1 --top-p 0.95 --top-k 20 --min-p 0 \
      --moe-cache-mib 4992 --port $PORT > "$log" 2>&1 &
  local pid=$!
  for i in $(seq 1 90); do
    [ "$(curl -s -m 2 http://127.0.0.1:$PORT/health)" = '{"status":"ok"}' ] && break
    sleep 5
  done
  local body; body=$(python3 -c 'import json;print(json.dumps({"prompt":"word "*7000,"n_predict":512,"seed":42,"temperature":0.8,"top_p":0.9,"typical_p":0.9,"repeat_penalty":1.1,"cache_prompt":False,"ignore_eos":True}))')
  for r in 1 2; do
    curl -s -m 900 http://127.0.0.1:$PORT/completion -H 'Content-Type: application/json' \
         --data "$body" > "$OUT/$tag-r$r.json"
    python3 - "$OUT/$tag-r$r.json" "$tag" "$r" <<'PY'
import json, sys
p, tag, r = sys.argv[1], sys.argv[2], sys.argv[3]
try:
    t = json.load(open(p)).get("timings", {})
    print(f"{tag} r{r} prefill={t.get('prompt_per_second',0):.2f} decode={t.get('predicted_per_second',0):.2f} n={t.get('predicted_n',0)}")
except Exception as e:
    print(f"{tag} r{r} FAILED {e}")
PY
  done
  kill "$pid" 2>/dev/null
  for i in $(seq 1 30); do ss -ltn 2>/dev/null | grep -q ":$PORT" || break; sleep 2; done
}

for p in $(seq 1 "$PAIRS"); do
  one "on-$p"  $ON
  one "off-$p" $OFF
done
echo "results in $OUT"
