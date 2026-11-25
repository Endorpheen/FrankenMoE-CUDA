#!/usr/bin/env bash
set -euo pipefail
src=$(cd "$(dirname "$0")" && pwd)
root=$(git rev-parse --show-toplevel)
out=${1:?Specify the diagnostic output directory}
previous="$root/build/bonsai-mtp-head-2025"
experiments="$src/../.."
cuda=${CUDA_HOME:-/usr/local/cuda}
mkdir -p "$out"
python3 "$src/prepare.py" "$previous/backend.c" "$out/backend.c"
gcc-13 -O2 -Wall -Wextra -Werror -ffunction-sections -fdata-sections -Wl,--gc-sections \
    -I"$src" -I"$previous" -I"$experiments/EXP-2026-11-25-2025-bonsai-mtp-head/source" \
    -I"$experiments/EXP-2026-11-25-2002-bonsai-session/source" -I"$cuda/include" \
    "$out/backend.c" "$previous/stem.o" \
    "$experiments/EXP-2026-11-25-1973-bonsai-pq2-bootstrap/source/bits.c" \
    -lm -ldl -pthread -o "$out/target-probe"
