#!/usr/bin/env bash
set -euo pipefail
src=$(cd "$(dirname "$0")" && pwd)
prev="$src/../../EXP-2026-11-25-2000-bonsai-mmq-k64/source"
out=${1:?Specify a new scratch directory}
cuda=${CUDA_HOME:-/usr/local/cuda}
bash "$prev/build.sh" "$out"
cat "$out/mmq-k64.cl" > "$out/mmq-k32.cl"
python3 "$src/compose.py" "$out/mmq-k64.cl" >> "$out/mmq-k32.cl"
clang-18 -target nvptx64-nvidia-cuda -march=sm_80 -cl-std=CL2.0 -x cl -O3 -ffp-contract=off \
    -nocudainc -nocudalib -Xclang -finclude-default-header -Xclang -mlink-builtin-bitcode \
    -Xclang "$cuda/nvvm/libdevice/libdevice.10.bc" -S "$out/mmq-k32.cl" -o "$out/mmq-k32.ptx"
"$cuda/bin/ptxas" -v -arch=sm_89 "$out/mmq-k32.ptx" -o "$out/mmq-k32.cubin" 2> "$out/mmq-k32.ptxas.log"
patch -s "$out/mmq-k64-backend.c" -o "$out/k32-backend.c" < "$src/backend.c.patch"
"${CC:-gcc-13}" -O2 -Wall -Wextra -Werror -Wl,--gc-sections -I"$out" -I"$cuda/include" \
    "$out/k32-backend.c" "$out/ffn-tail.o" \
    "$src/../../EXP-2026-11-25-1973-bonsai-pq2-bootstrap/source/bits.c" \
    -l:libcuda.so.1 -lm -pthread -o "$out/bonsai-mmq-k32"
