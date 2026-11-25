#!/usr/bin/env bash
set -euo pipefail
src=$(cd "$(dirname "$0")" && pwd)
root=$(git rev-parse --show-toplevel)
out=${1:?Specify a new scratch directory}
device=${2:?Specify the built D4 image directory}
base="$root/build/bonsai-row-reuse-2006-http"
host="$root/build/bonsai-mmq-k64-2000-default"
quality="$src/../../EXP-2026-11-25-1994-bonsai-pq2-int8/source"
session="$src/../../EXP-2026-11-25-2002-bonsai-session/source"
previous="$src/../../EXP-2026-11-25-2009-bonsai-selective-ffn/source"
cuda=${CUDA_HOME:-/usr/local/cuda}
mkdir "$out"
python3 "$previous/compose.py" "$base/backend.c" "$out/previous.c"
python3 "$src/quality-host.py" "$out/previous.c" "$out/backend.c"
python3 "$quality/instrument-logits.py" "$out/backend.c" "$out/quality.c"
sed -i 's/i<4 || i%8==0 || i+1==generate/i<4 || i%8==0 || i==59 || i==195 || i+1==generate/' "$out/quality.c"
gcc-13 -O2 -Wall -Wextra -Werror -ffunction-sections -fdata-sections -Wl,--gc-sections \
    -I"$host" -I"$session" -I"$cuda/include" "$out/quality.c" "$host/ffn-tail.o" \
    "$src/../../EXP-2026-11-25-1973-bonsai-pq2-bootstrap/source/bits.c" \
    -l:libcuda.so.1 -lm -pthread -o "$out/bonsai-quality"
ln -s "$(realpath "$device/d4.cubin")" "$out/pq2-int8.cubin"
ln -s "$host/frozen-goose.ptx" "$out/frozen-goose.ptx"
