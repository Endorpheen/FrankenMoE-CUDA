#!/usr/bin/env bash
set -euo pipefail
src=$(cd "$(dirname "$0")" && pwd)
root=$(git rev-parse --show-toplevel)
out=${1:?Specify a new scratch directory}
base="$root/build/bonsai-row-reuse-2006-http"
host="$root/build/bonsai-mmq-k64-2000-default"
i8="$src/../../EXP-2026-11-25-1994-bonsai-pq2-int8/source"
residual="$src/../../EXP-2026-11-25-1996-bonsai-pq2-q8-residual/source"
session="$src/../../EXP-2026-11-25-2002-bonsai-session/source"
cuda=${CUDA_HOME:-/usr/local/cuda}
mkdir "$out"
cat "$base/rows.cl" "$i8/pq2-int8.cl" > "$out/selective.cl"
python3 "$residual/compose.py" "$i8/pq2-int8.cl" >> "$out/selective.cl"
clang-18 -target nvptx64-nvidia-cuda -march=sm_80 -cl-std=CL2.0 -x cl -O3 -ffp-contract=off \
    -nocudainc -nocudalib -Xclang -finclude-default-header -Xclang -mlink-builtin-bitcode \
    -Xclang "$cuda/nvvm/libdevice/libdevice.10.bc" -S "$out/selective.cl" -o "$out/selective.ptx"
"$cuda/bin/ptxas" -v -arch=sm_89 "$out/selective.ptx" -o "$out/pq2-int8.cubin" 2>"$out/ptxas.txt"
python3 "$src/compose.py" "$base/backend.c" "$out/backend.c"
python3 "$i8/instrument-logits.py" "$out/backend.c" "$out/quality.c"
sed -i 's/i<4 || i%8==0 || i+1==generate/i<4 || i%8==0 || i==59 || i==195 || i+1==generate/' "$out/quality.c"
gcc-13 -O2 -Wall -Wextra -Werror -ffunction-sections -fdata-sections -Wl,--gc-sections \
    -I"$host" -I"$session" -I"$cuda/include" "$out/quality.c" "$host/ffn-tail.o" \
    "$src/../../EXP-2026-11-25-1973-bonsai-pq2-bootstrap/source/bits.c" \
    -l:libcuda.so.1 -lm -pthread -o "$out/bonsai-quality"
ln -s "$host/frozen-goose.ptx" "$out/frozen-goose.ptx"
