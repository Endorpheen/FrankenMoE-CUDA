#!/usr/bin/env bash
set -euo pipefail
src=$(cd "$(dirname "$0")" && pwd)
root=$(git rev-parse --show-toplevel)
out=${1:?Specify a diagnostic output directory}
base="$root/build/bonsai-mtp-choice-2034"
attention="$root/build/bonsai-mtp-attn-2033"
prefill="$root/build/bonsai-mtp-prefill-2032"
transaction="$root/build/bonsai-mtp-commit-2030"
experiments="$src/../.."
cuda=${CUDA_HOME:-/usr/local/cuda}
mkdir -p "$out"
python3 "$src/prepare.py" "$base/backend.c" \
    "$experiments/EXP-2026-11-25-2034-bonsai-mtp-device-argmax/source/greedy-device.inc" "$out"
cp "$base/greedy.inc" "$out/greedy.inc"
gcc-13 -O2 -Wall -Wextra -Werror -ffunction-sections -fdata-sections -Wl,--gc-sections \
    -I"$out" -I"$base" -I"$attention" -I"$prefill" -I"$transaction" \
    -I"$experiments/EXP-2026-11-25-2034-bonsai-mtp-device-argmax/source" \
    -I"$experiments/EXP-2026-11-25-2032-bonsai-mtp-batch-prefill/source" \
    -I"$experiments/EXP-2026-11-25-2030-bonsai-mtp-device-commit/source" \
    -I"$experiments/EXP-2026-11-25-2025-bonsai-mtp-head/source" \
    -I"$experiments/EXP-2026-11-25-2002-bonsai-session/source" -I"$cuda/include" \
    "$out/profile-backend.c" "$prefill/greedy.o" \
    "$experiments/EXP-2026-11-25-1973-bonsai-pq2-bootstrap/source/bits.c" \
    -lm -ldl -pthread -o "$out/bonsai-mtp-profile"
