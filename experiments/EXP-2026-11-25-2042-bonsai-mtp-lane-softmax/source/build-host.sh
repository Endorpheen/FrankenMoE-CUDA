#!/usr/bin/env bash
set -euo pipefail
src=$(cd "$(dirname "$0")" && pwd)
root=$(git rev-parse --show-toplevel)
out=${1:?Specify a diagnostic output directory}
base="$root/build/bonsai-flash-qpack-2041-linear"
head="$root/build/bonsai-mtp-head-2038"
merge="$root/build/bonsai-mtp-merge-2036"
choice="$root/build/bonsai-mtp-choice-2034"
prefill="$root/build/bonsai-mtp-prefill-2032"
transaction="$root/build/bonsai-mtp-commit-2030"
experiments="$src/../.."
cuda=${CUDA_HOME:-/usr/local/cuda}
mkdir -p "$out"
python3 "$src/prepare.py" "$base/backend.c" "$merge/verify-ops.inc" "$out"
gcc-13 -O2 -Wall -Wextra -Werror -ffunction-sections -fdata-sections -Wl,--gc-sections \
    -I"$out" -I"$head" -I"$merge" -I"$choice" -I"$prefill" -I"$transaction" \
    -I"$experiments/EXP-2026-11-25-2034-bonsai-mtp-device-argmax/source" \
    -I"$experiments/EXP-2026-11-25-2032-bonsai-mtp-batch-prefill/source" \
    -I"$experiments/EXP-2026-11-25-2030-bonsai-mtp-device-commit/source" \
    -I"$experiments/EXP-2026-11-25-2002-bonsai-session/source" -I"$cuda/include" \
    "$out/backend.c" "$prefill/greedy.o" \
    "$experiments/EXP-2026-11-25-1973-bonsai-pq2-bootstrap/source/bits.c" \
    -lm -ldl -pthread -o "$out/bonsai-mtp-tail"
