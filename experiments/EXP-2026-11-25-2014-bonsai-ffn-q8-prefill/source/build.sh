#!/usr/bin/env bash
set -euo pipefail
src=$(cd "$(dirname "$0")" && pwd)
root=$(git rev-parse --show-toplevel)
out=${1:?Specify a new scratch directory}
base="$root/build/bonsai-wide-norm-2013-http"
dev="$root/build/bonsai-wide-norm-2013"
host="$root/build/bonsai-mmq-k64-2000-default"
cuda=${CUDA_HOME:-/usr/local/cuda}
exp="$src/../.."
prior="$exp/EXP-2026-11-25-1994-bonsai-pq2-int8/source"
session="$exp/EXP-2026-11-25-2002-bonsai-session/source"
mkdir "$out"
cat "$dev/norm.cl" "$prior/pq2-int8.cl" > "$out/ffn-q8.cl"
clang-18 -target nvptx64-nvidia-cuda -march=sm_80 -cl-std=CL2.0 -x cl -O3 -ffp-contract=off \
    -nocudainc -nocudalib -Xclang -finclude-default-header -Xclang -mlink-builtin-bitcode \
    -Xclang "$cuda/nvvm/libdevice/libdevice.10.bc" -S "$out/ffn-q8.cl" -o "$out/ffn-q8.ptx"
"$cuda/bin/ptxas" -v -arch=sm_89 "$out/ffn-q8.ptx" -o "$out/pq2-int8.cubin" 2>"$out/ptxas.txt"
python3 "$src/compose.py" "$base/backend.c" "$out/backend.c"
python3 "$prior/instrument-logits.py" "$out/backend.c" "$out/quality.c"
gcc-13 -O2 -Wall -Wextra -Werror -ffunction-sections -fdata-sections -Wl,--gc-sections \
    -I"$host" -I"$session" -I"$cuda/include" "$out/quality.c" "$host/ffn-tail.o" \
    "$exp/EXP-2026-11-25-1973-bonsai-pq2-bootstrap/source/bits.c" -l:libcuda.so.1 -lm -pthread \
    -o "$out/bonsai-quality"
ln -s "$host/frozen-goose.ptx" "$out/frozen-goose.ptx"
