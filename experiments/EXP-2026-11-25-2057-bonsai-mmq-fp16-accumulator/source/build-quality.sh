#!/usr/bin/env bash
set -euo pipefail

src=$(cd "$(dirname "$0")" && pwd)
root=$(git -C "$src" rev-parse --show-toplevel)
out=${1:?Specify the component build directory}
base=${2:?Specify EXP-2049 build directory}
artifacts=${FRANKENMOE_CHECKPOINTS:-$root/build}
cuda=${CUDA_HOME:-/usr/local/cuda}
host="$artifacts/bonsai-mmq-k64-2000-default"
session="$root/experiments/EXP-2026-11-25-2002-bonsai-session/source"
experiment="$root/experiments/EXP-2026-11-25-1994-bonsai-pq2-int8/source"

mkdir "$out/quality-fixture"
python3 "$src/prepare-host.py" "$base/backend.c" "$out/quality-host.c"
python3 "$experiment/instrument-logits.py" "$out/quality-host.c" "$out/quality-logits.c"
gcc-13 -O2 -Wall -Wextra -Werror -Wl,--gc-sections \
    -I"$host" -I"$session" -I"$cuda/include" "$out/quality-logits.c" "$host/ffn-tail.o" \
    "$root/experiments/EXP-2026-11-25-1973-bonsai-pq2-bootstrap/source/bits.c" \
    -ldl -lm -pthread -o "$out/quality-fixture/bonsai-quality"
ln -s ../pipeline.cubin "$out/quality-fixture/pq2-int8.cubin"
ln -s "$(realpath "$base/package/cuda/frozen-goose.ptx")" "$out/quality-fixture/frozen-goose.ptx"
