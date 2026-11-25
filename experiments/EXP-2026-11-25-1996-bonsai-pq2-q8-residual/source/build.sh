#!/usr/bin/env bash
set -euo pipefail
src=$(cd "$(dirname "$0")" && pwd)
prev="$src/../../EXP-2026-11-25-1994-bonsai-pq2-int8/source"
out=${1:?Specify a new scratch directory}
cuda=${CUDA_HOME:-/usr/local/cuda}
bash "$prev/build.sh" "$out"
cat "$out/pq2-int8.cl" > "$out/pq2-residual.cl"
python3 "$src/compose.py" "$out/pq2-int8.cl" >> "$out/pq2-residual.cl"
clang-18 -target nvptx64-nvidia-cuda -march=sm_80 -cl-std=CL2.0 -x cl -O3 -ffp-contract=off \
   -nocudainc -nocudalib -Xclang -finclude-default-header -Xclang -mlink-builtin-bitcode \
   -Xclang "$cuda/nvvm/libdevice/libdevice.10.bc" -S "$out/pq2-residual.cl" -o "$out/pq2-residual.ptx"
"$cuda/bin/ptxas" -v -arch=sm_89 "$out/pq2-residual.ptx" -o "$out/pq2-residual.cubin" 2> "$out/pq2-residual.ptxas.log"
patch -s "$out/int8-backend.c" -o "$out/residual-backend.c" < "$src/backend.c.patch"
"${CC:-gcc-13}" -O2 -Wall -Wextra -Werror -Wl,--gc-sections -I"$out" -I"$cuda/include" \
    "$out/residual-backend.c" "$out/bonsai.o" \
    "$src/../../EXP-2026-11-25-1973-bonsai-pq2-bootstrap/source/bits.c" \
    -l:libcuda.so.1 -lm -pthread -o "$out/bonsai-mmq-residual"
python3 "$prev/instrument-logits.py" "$out/residual-backend.c" "$out/quality-residual-backend.c"
"${CC:-gcc-13}" -O2 -Wall -Wextra -Werror -Wl,--gc-sections -I"$out" -I"$cuda/include" \
    "$out/quality-residual-backend.c" "$out/bonsai.o" \
    "$src/../../EXP-2026-11-25-1973-bonsai-pq2-bootstrap/source/bits.c" \
    -l:libcuda.so.1 -lm -pthread -o "$out/bonsai-quality"
mkdir "$out/quality-fixture"
ln -s ../bonsai-quality "$out/quality-fixture/bonsai-quality"
ln -s ../pq2-residual.cubin "$out/quality-fixture/pq2-int8.cubin"
ln -s ../frozen-goose.ptx "$out/quality-fixture/frozen-goose.ptx"
