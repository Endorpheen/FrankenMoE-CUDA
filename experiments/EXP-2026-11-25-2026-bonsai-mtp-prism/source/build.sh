#!/usr/bin/env bash
set -euo pipefail
src=$(cd "$(dirname "$0")" && pwd)
root=$(git rev-parse --show-toplevel)
out=${1:?Specify the diagnostic output directory}
ref=${PRISM_REPO:-/home/end0/AI/LlamaServer/bonsai-prism-reference-20261125}
cuda=${CUDA_HOME:-/usr/local/cuda}
previous="$root/build/bonsai-mtp-head-2025"
experiments="$src/../.."
mkdir -p "$out"
python3 "$src/prepare-goose.py" \
    "$experiments/EXP-2026-11-25-2025-bonsai-mtp-head/source/head-probe.inc" "$out/head-probe.inc"
gcc-13 -O2 -Wall -Wextra -Werror -ffunction-sections -fdata-sections -Wl,--gc-sections \
    -I"$out" -I"$previous" -I"$experiments/EXP-2026-11-25-2002-bonsai-session/source" -I"$cuda/include" \
    "$previous/backend.c" "$previous/stem.o" \
    "$experiments/EXP-2026-11-25-1973-bonsai-pq2-bootstrap/source/bits.c" \
    -lm -ldl -pthread -o "$out/goose-replay"
gcc-13 -O2 -Wall -Wextra -Werror -I"$ref/include" -I"$ref/ggml/include" \
    "$src/prism-replay.c" -L"$ref/build-sm89/bin" -Wl,-rpath,"$ref/build-sm89/bin" \
    "$ref/build-sm89/bin/libllama.so.0.7.0" "$ref/build-sm89/bin/libggml-base.so.0.27.0" "$ref/build-sm89/bin/libggml.so.0.27.0" -o "$out/prism-replay"
