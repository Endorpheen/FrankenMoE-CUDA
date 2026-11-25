#!/usr/bin/env bash
set -euo pipefail
src=$(cd "$(dirname "$0")" && pwd)
prev="$src/../../EXP-2026-11-25-1991-bonsai-gdn-mma/source"
out=${1:?Specify a new scratch directory}
cuda=${CUDA_HOME:-/usr/local/cuda}
bash "$prev/build.sh" "$out"
cat "$out/pq2-mmq-loads.cl" > "$out/pq2-mmq-wide.cl"
python3 "$src/compose.py" "$out/pq2-mmq-loads.cl" >> "$out/pq2-mmq-wide.cl"
clang-18 -target nvptx64-nvidia-cuda -march=sm_80 -cl-std=CL2.0 -x cl -O3 -ffp-contract=off \
   -nocudainc -nocudalib -Xclang -finclude-default-header -Xclang -mlink-builtin-bitcode \
   -Xclang "$cuda/nvvm/libdevice/libdevice.10.bc" -S "$out/pq2-mmq-wide.cl" -o "$out/pq2-mmq-wide.ptx"
"$cuda/bin/ptxas" -v -arch=sm_89 "$out/pq2-mmq-wide.ptx" -o "$out/pq2-mmq-wide.cubin" 2> "$out/pq2-mmq-wide.ptxas.log"
patch -s "$out/gdn-backend.c" -o "$out/mmq-wide-backend.c" < "$src/backend.c.patch"
"${CC:-gcc-13}" -O2 -Wall -Wextra -Werror -Wl,--gc-sections -I"$out" -I"$cuda/include" \
    "$out/mmq-wide-backend.c" "$out/bonsai.o" \
    "$src/../../EXP-2026-11-25-1973-bonsai-pq2-bootstrap/source/bits.c" \
    -l:libcuda.so.1 -lm -pthread -o "$out/bonsai-mmq-wide"
