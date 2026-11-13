#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${BUILD_DIR:-${ROOT_DIR}/build/franken-cuda}"
MODEL_PATH="${1:-${MODEL_PATH:-}}"
if [[ -z "${MODEL_PATH}" ]]; then
    echo "Usage: $0 /path/to/small-moe.gguf" >&2
    exit 2
fi

mkdir -p "${ROOT_DIR}/results"
CLI="${BUILD_DIR}/cli/bmoe-cli"
COMMON=(-m "${MODEL_PATH}" -p "benchmark prompt" -n "${N_PREDICT:-128}" -c "${CTX_SIZE:-256}" --ubatch 32)

run_case() {
    local name="$1"
    shift
    local csv="${ROOT_DIR}/results/${name}.csv"
    "${CLI}" "${COMMON[@]}" --csv "${csv}" "$@" >/dev/null 2>"${ROOT_DIR}/results/${name}.log"
    "${ROOT_DIR}/scripts/metrics_to_json.py" "${csv}" "${ROOT_DIR}/results/${name}.json"
}

run_case resident
run_case bmoe-cpu --moe-stream --cache-mb 2 --force-cache --dense-weights anon --io-threads 2
run_case expert-tier --gpu-layers -1 --expert-hot-slots "${HOT_SLOTS:-2}"
run_case ssd-ram-vram --gpu-layers -1 --expert-hot-slots "${HOT_SLOTS:-2}" \
    --moe-stream --cache-mb 2 --force-cache --dense-weights anon --io-threads 2 --overlap

printf '| Mode | decode tok/s | SSD MiB | H2D MiB/tok | RAM cache MiB | VRAM cache MiB |\n'
printf '|---|---:|---:|---:|---:|---:|\n'
for name in resident bmoe-cpu expert-tier ssd-ram-vram; do
    jq -r --arg n "${name}" \
        '"| \($n) | \(."tok/s") | \(.read_MiB) | \(."h2d_MiB/tok") | \(.cache_resident_MiB) | \(.vram_cache_MiB) |"' \
        "${ROOT_DIR}/results/${name}.json"
done
