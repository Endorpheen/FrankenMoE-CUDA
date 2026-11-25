#!/usr/bin/env bash
set -euo pipefail
src=$(cd "$(dirname "$0")" && pwd)
out=${1:?Specify a new scratch directory}
dev=${2:?Specify the DP4A build directory}
root=$(git rev-parse --show-toplevel)
base="$root/build/bonsai-flash-q-reuse-2011-http"
host="$root/build/bonsai-mmq-k64-2000-default"
exp="$src/../.."
cuda=${CUDA_HOME:-/usr/local/cuda}
mkdir "$out"
python3 "$src/compose.py" "$base/backend.c" "$out/backend.c"
python3 "$exp/EXP-2026-11-25-1994-bonsai-pq2-int8/source/instrument-logits.py" "$out/backend.c" "$out/quality.c"
gcc-13 -O2 -Wall -Wextra -Werror -ffunction-sections -fdata-sections -Wl,--gc-sections \
    -I"$host" -I"$exp/EXP-2026-11-25-2002-bonsai-session/source" -I"$cuda/include" \
    "$out/quality.c" "$host/ffn-tail.o" "$exp/EXP-2026-11-25-1973-bonsai-pq2-bootstrap/source/bits.c" \
    -l:libcuda.so.1 -lm -pthread -o "$out/bonsai-quality"
ln -s "$(realpath "$dev/mvq.cubin")" "$out/pq2-int8.cubin"
ln -s "$host/frozen-goose.ptx" "$out/frozen-goose.ptx"
