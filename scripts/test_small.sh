#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${BUILD_DIR:-${ROOT_DIR}/build/franken-cuda}"
MODEL_PATH="${1:-${MODEL_PATH:-${BUILD_DIR}/tests/tiny-moe-qwen3moe.gguf}}"
CLI="${BUILD_DIR}/cli/bmoe-cli"

# KEEP_TMP=1 preserves CLI output, logs, and token files for diagnosis.
KEEP_TMP="${KEEP_TMP:-0}"

ctest --test-dir "${BUILD_DIR}" --output-on-failure

if ! command -v nvidia-smi >/dev/null || ! nvidia-smi -L >/dev/null 2>&1; then
    echo "Skipping CUDA gate: no NVIDIA GPU is available to this process" >&2
    exit 0
fi

TMP_DIR="$(mktemp -d)"
if [[ "${KEEP_TMP}" == "1" ]]; then
    echo "KEEP_TMP=1: run artifacts will remain in ${TMP_DIR}" >&2
    trap 'echo "KEEP_TMP=1: artifacts are in ${TMP_DIR}" >&2' EXIT
else
    trap 'rm -rf "${TMP_DIR}"' EXIT
fi

# Print the failing log tail while preserving the original exit status.
report_failure() {
    local step="$1" rc="$2" log="$3"
    echo "FAILED STEP: ${step} (exit ${rc})" >&2
    if [[ -f "${log}" ]]; then
        echo "--- last 40 lines of ${log} ---" >&2
        tail -n 40 "${log}" >&2 || true
    fi
    exit "${rc}"
}

COMMON=(-m "${MODEL_PATH}" -p lossless-check -n 64 -c 128 --ubatch 8 --gpu-layers -1)

set +e
"${CLI}" "${COMMON[@]}" >"${TMP_DIR}/resident.out" 2>"${TMP_DIR}/resident.log"
rc=$?
set -e
[[ ${rc} -eq 0 ]] || report_failure "resident run" "${rc}" "${TMP_DIR}/resident.log"

set +e
"${CLI}" "${COMMON[@]}" --moe-stream --cache-mb 2 --force-cache \
    --expert-hot-slots 2 --dense-weights anon --io-threads 2 \
    >"${TMP_DIR}/tiered.out" 2>"${TMP_DIR}/tiered.log"
rc=$?
set -e
[[ ${rc} -eq 0 ]] || report_failure "tiered run" "${rc}" "${TMP_DIR}/tiered.log"

# Ignore the timing report after the second blank line and compare only the prompt plus generated
# byte stream, which is the actual token-sequence contract.
perl -0777 -ne 'print $1 if /\A(.*?)\n\ngeneration:/s' "${TMP_DIR}/resident.out" >"${TMP_DIR}/resident.tokens"
perl -0777 -ne 'print $1 if /\A(.*?)\n\ngeneration:/s' "${TMP_DIR}/tiered.out" >"${TMP_DIR}/tiered.tokens"

set +e
cmp "${TMP_DIR}/resident.tokens" "${TMP_DIR}/tiered.tokens"
rc=$?
set -e
if [[ ${rc} -ne 0 ]]; then
    echo "lossless mismatch: resident vs tiered (cmp exit ${rc})" >&2
    echo "sizes: resident $(stat -c %s "${TMP_DIR}/resident.tokens") bytes, tiered $(stat -c %s "${TMP_DIR}/tiered.tokens") bytes" >&2
    echo "--- first differing bytes: offset resident tiered ---" >&2
    cmp -l "${TMP_DIR}/resident.tokens" "${TMP_DIR}/tiered.tokens" | head -n 10 >&2 || true
    echo "--- resident.tokens prefix ---" >&2
    hexdump -C "${TMP_DIR}/resident.tokens" | head -n 4 >&2 || true
    echo "--- tiered.tokens prefix ---" >&2
    hexdump -C "${TMP_DIR}/tiered.tokens" | head -n 4 >&2 || true
    echo "--- resident.log tail ---" >&2
    tail -n 20 "${TMP_DIR}/resident.log" >&2 || true
    echo "--- tiered.log tail ---" >&2
    tail -n 20 "${TMP_DIR}/tiered.log" >&2 || true
    exit "${rc}"
fi

set +e
"${CLI}" -m "${MODEL_PATH}" -p overlap-check -n 32 -c 64 --ubatch 8 \
    --gpu-layers -1 --moe-stream --cache-mb 2 --force-cache --expert-hot-slots 2 \
    --dense-weights anon --io-threads 2 --overlap >"${TMP_DIR}/overlap.out" 2>"${TMP_DIR}/overlap.log"
rc=$?
set -e
[[ ${rc} -eq 0 ]] || report_failure "overlap run" "${rc}" "${TMP_DIR}/overlap.log"

echo "Small tests passed: CTest, lossless CUDA equality, and overlap"
