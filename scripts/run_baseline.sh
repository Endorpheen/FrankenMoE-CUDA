#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
MODEL_PATH="${1:-${MODEL_PATH:-${ROOT_DIR}/models/qwen38/UD-IQ3_XXS/Qwen3.8-Flash-Next-UD-IQ3_XXS-00001-of-00003.gguf}}"
RUN_DIR="${RUN_DIR:-${ROOT_DIR}/benchmarks/runs}"
RUNS="${RUNS:-5}"

mkdir -p "${RUN_DIR}"
for i in $(seq 1 "${RUNS}"); do
    run_id="run-$(printf '%02d' "${i}")"
    CSV_PATH="${RUN_DIR}/${run_id}.csv" \
    MONITOR_PATH="${RUN_DIR}/${run_id}.monitor.csv" \
    JSON_PATH="${RUN_DIR}/${run_id}.json" \
    N_PREDICT=256 CTX_SIZE=4096 UBATCH=256 THREADS=16 IO_THREADS=4 \
    RAM_CACHE_CEIL_MB=32768 VRAM_CACHE_MB=2048 VRAM_RESERVE_MB=4096 \
        "${ROOT_DIR}/scripts/run_qwen38.sh" "${MODEL_PATH}" \
        >"${RUN_DIR}/${run_id}.out" 2>"${RUN_DIR}/${run_id}.log"
done

python3 "${ROOT_DIR}/scripts/aggregate_baseline.py" "${RUN_DIR}" "${ROOT_DIR}/benchmarks/baseline.json"
