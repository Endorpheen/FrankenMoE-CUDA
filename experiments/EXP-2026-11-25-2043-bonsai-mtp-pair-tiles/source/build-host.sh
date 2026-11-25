#!/usr/bin/env bash
set -euo pipefail
src=$(cd "$(dirname "$0")" && pwd)
root=$(git rev-parse --show-toplevel)
out=${1:?Specify a diagnostic output directory}
base="$root/build/bonsai-mtp-softmax-2042"
head="$root/build/bonsai-mtp-head-2038"
prefill="$root/build/bonsai-mtp-prefill-2032"
experiments="$src/../.."
cuda=${CUDA_HOME:-/usr/local/cuda}
choice="$experiments/EXP-2026-11-25-2034-bonsai-mtp-device-argmax/source"
transaction="$experiments/EXP-2026-11-25-2030-bonsai-mtp-device-commit/source"
mkdir -p "$out"
python3 "$src/prepare.py" "$head/greedy.inc" "$base/backend.c" "$base/verify-ops.inc" \
    "$choice" "$transaction" "$experiments" "$out"
gcc-13 -O2 -Wall -Wextra -Werror -ffunction-sections -fdata-sections -Wl,--gc-sections \
    -I"$out" -I"$head" -I"$prefill" \
    -I"$experiments/EXP-2026-11-25-2032-bonsai-mtp-batch-prefill/source" \
    -I"$experiments/EXP-2026-11-25-2002-bonsai-session/source" -I"$cuda/include" \
    "$out/backend.c" "$prefill/greedy.o" \
    "$experiments/EXP-2026-11-25-1973-bonsai-pq2-bootstrap/source/bits.c" \
    -lm -ldl -pthread -o "$out/bonsai-mtp-pairs"
