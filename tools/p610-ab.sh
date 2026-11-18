#!/usr/bin/env bash
# Triple paired A/B of the Goose sampler stages on the accepted profile.
# Both sides use the same server binary, the same library and the same profile;
# only the stage flags differ. Each side sends three requests and records the
# server timings plus VRAM and RSS.
set -uo pipefail
R=/home/end0/AI/FrankenMoE-CUDA
S=/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108/runtime/reconstructed/build/stage-run/llama-server
M=/home/end0/AI/LocalAI/Models/Apex-35B-V1.0-Q4_K_M.gguf
OUT=${1:-$R/results/p610-ab}
mkdir -p "$OUT"
PORT=9101

req() {
  python3 - "$1" <<'PY'
import json, sys
prompt = "word " * 7000          # roughly 32K characters, the profile's input size
print(json.dumps({"prompt": prompt, "n_predict": 512, "seed": 42,
                  "temperature": 0.8, "top_p": 0.9, "typical_p": 0.9,
                  "repeat_penalty": 1.1, "cache_prompt": False, "ignore_eos": True}))
PY
}

run_side() {
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
  local vram_after_load
  vram_after_load=$(nvidia-smi --query-gpu=memory.used --format=csv,noheader,nounits)
  for r in 1 2 3; do
    curl -s -m 900 http://127.0.0.1:$PORT/completion -H 'Content-Type: application/json' \
         --data "$(req 7000)" > "$OUT/$tag-r$r.json"
    python3 - "$OUT/$tag-r$r.json" "$tag" "$r" <<'PY'
import json, sys
p, tag, r = sys.argv[1], sys.argv[2], sys.argv[3]
try:
    d = json.load(open(p))
    t = d.get("timings", {})
    print(f"{tag} r{r} prefill={t.get('prompt_per_second',0):.2f} decode={t.get('predicted_per_second',0):.2f} "
          f"prompt_n={t.get('prompt_n',0)} predicted_n={t.get('predicted_n',0)}")
except Exception as e:
    print(f"{tag} r{r} FAILED {e}")
PY
  done
  local vram_peak rss
  vram_peak=$(nvidia-smi --query-gpu=memory.used --format=csv,noheader,nounits)
  rss=$(ps -o rss= -p "$pid" | tr -d ' ')
  printf '%s vram_load=%s vram_peak=%s rss_kb=%s goose_calls=%s\n' \
      "$tag" "$vram_after_load" "$vram_peak" "${rss:-0}" "$(grep -c FG171 "$log")"
  kill "$pid" 2>/dev/null
  for i in $(seq 1 30); do ss -ltn 2>/dev/null | grep -q ":$PORT" || break; sleep 2; done
}

run_side "on"  GGML_GOOSE_SAMPLER_CHAIN=1
run_side "off" GGML_GOOSE_SAMPLER_CHAIN=0 GGML_GOOSE_SAMPLER_TOP_P=0 GGML_GOOSE_SAMPLER_DRY=0
echo "results in $OUT"
