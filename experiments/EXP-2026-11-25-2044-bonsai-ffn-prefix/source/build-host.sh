#!/usr/bin/env bash
set -euo pipefail
src=$(cd "$(dirname "$0")" && pwd)
root=$(git rev-parse --show-toplevel)
out=${1:?Specify a diagnostic output directory}
base="$root/build/bonsai-mtp-pairs-2043"
head="$root/build/bonsai-mtp-head-2038"
prefill="$root/build/bonsai-mtp-prefill-2032"
experiments="$src/../.."
cuda=${CUDA_HOME:-/usr/local/cuda}
compiler=${GOOSE_BIN:-/home/end0/AI/Fried-Goose/releases/fried-goose-v0.3.0-preview-keelwing/bin/goose}
mkdir -p "$out"
python3 "$src/prepare.py" "$base/backend.c" "$prefill/greedy.goose" "$out"
"$compiler" --standalone -o "$out/prefix.c" --header "$out/bonsai.h" "$out/prefix.goose"
gcc-13 -O2 -fPIC -ffunction-sections -fdata-sections -w -c "$out/prefix.c" -o "$out/prefix.o"
gcc-13 -O2 -Wall -Wextra -Werror -ffunction-sections -fdata-sections -Wl,--gc-sections \
    -I"$out" -I"$base" -I"$head" -I"$prefill" \
    -I"$experiments/EXP-2026-11-25-2032-bonsai-mtp-batch-prefill/source" \
    -I"$experiments/EXP-2026-11-25-2002-bonsai-session/source" -I"$cuda/include" \
    "$out/backend.c" "$out/prefix.o" \
    "$experiments/EXP-2026-11-25-1973-bonsai-pq2-bootstrap/source/bits.c" \
    -lm -ldl -pthread -o "$out/bonsai-ffn-prefix"
