#!/usr/bin/env bash
set -euo pipefail
src=$(cd "$(dirname "$0")" && pwd)
prev="$src/../../EXP-2026-11-25-1986-bonsai-flash-ordered-merge/source"
out=${1:?Specify a new scratch directory}
cuda=${CUDA_HOME:-/usr/local/cuda}
bash "$prev/build.sh" "$out"
cat "$out/flash-ordered-merge.cl" > "$out/flash-compact.cl"
python3 "$src/compose.py" "$out/flash-ordered-merge.cl" >> "$out/flash-compact.cl"
clang-18 -target nvptx64-nvidia-cuda -march=sm_80 -cl-std=CL2.0 -x cl -O3 -ffp-contract=off \
   -nocudainc -nocudalib -Xclang -finclude-default-header -Xclang -mlink-builtin-bitcode \
   -Xclang "$cuda/nvvm/libdevice/libdevice.10.bc" -S "$out/flash-compact.cl" -o "$out/flash-compact.ptx"
"$cuda/bin/ptxas" -v -arch=sm_89 "$out/flash-compact.ptx" -o "$out/flash-compact.cubin" 2> "$out/flash-compact.ptxas.log"
patch -s "$out/ordered-merge-backend.c" -o "$out/compact-backend.c" < "$src/backend.c.patch"
"${CC:-gcc-13}" -O2 -Wall -Wextra -Werror -Wl,--gc-sections -I"$out" -I"$cuda/include" \
    "$out/compact-backend.c" "$out/bonsai.o" \
    "$src/../../EXP-2026-11-25-1973-bonsai-pq2-bootstrap/source/bits.c" \
    -l:libcuda.so.1 -lm -pthread -o "$out/bonsai-flash-compact"
