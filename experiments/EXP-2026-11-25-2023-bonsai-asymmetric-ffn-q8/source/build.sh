#!/usr/bin/env bash
set -euo pipefail
src=$(cd "$(dirname "$0")" && pwd)
root=$(git rev-parse --show-toplevel)
out=${1:?Specify a new scratch directory}
base="$root/build/bonsai-mmq-pipeline-2019-http"
host="$root/build/bonsai-mmq-k64-2000-default"
cuda=${CUDA_HOME:-/usr/local/cuda}
exp="$src/../.."
prior="$exp/EXP-2026-11-25-1994-bonsai-pq2-int8/source"
session="$exp/EXP-2026-11-25-2002-bonsai-session/source"
mkdir "$out"
python3 "$src/compose.py" "$base/backend.c" "$out/backend.c"
python3 "$prior/instrument-logits.py" "$out/backend.c" "$out/quality.c"
gcc-13 -O2 -Wall -Wextra -Werror -ffunction-sections -fdata-sections -Wl,--gc-sections \
    -I"$host" -I"$session" -I"$cuda/include" "$out/quality.c" "$host/ffn-tail.o" \
    "$exp/EXP-2026-11-25-1973-bonsai-pq2-bootstrap/source/bits.c" -lm -ldl -pthread \
    -o "$out/bonsai-quality"
ln -s "$base/pipeline.cubin" "$out/pq2-int8.cubin"
ln -s "$host/frozen-goose.ptx" "$out/frozen-goose.ptx"
