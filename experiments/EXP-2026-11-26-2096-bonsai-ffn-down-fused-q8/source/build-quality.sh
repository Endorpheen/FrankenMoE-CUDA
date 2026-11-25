#!/usr/bin/env bash
set -euo pipefail

src=$(cd "$(dirname "$0")" && pwd)
root=$(git -C "$src" rev-parse --show-toplevel)
out=${1:?Specify a fresh fixture directory}
base=${2:?Specify EXP-2096 build directory}
package=${3:?Specify staged EXP-2095 package}
artifacts=${FRANKENMOE_CHECKPOINTS:-$root/build}
cuda=${CUDA_HOME:-/usr/local/cuda}
host="$artifacts/bonsai-mmq-k64-2000-default"
session="$root/experiments/EXP-2026-11-25-2002-bonsai-session/source"
experiment="$root/experiments/EXP-2026-11-25-1994-bonsai-pq2-int8/source"

mkdir "$out"
python3 "$experiment/instrument-logits.py" "$base/backend.c" "$out/logits.c"
gcc-13 -O2 -Wall -Wextra -Werror -Wl,--gc-sections \
    -I"$host" -I"$session" -I"$cuda/include" "$out/logits.c" "$host/ffn-tail.o" \
    "$root/experiments/EXP-2026-11-25-1973-bonsai-pq2-bootstrap/source/bits.c" \
    -ldl -lm -pthread -o "$out/bonsai-quality"
ln -s "$(realpath "$base/pipeline.cubin")" "$out/pq2-int8.cubin"
ln -s "$(realpath "$package/cuda/frozen-goose.ptx")" "$out/frozen-goose.ptx"
