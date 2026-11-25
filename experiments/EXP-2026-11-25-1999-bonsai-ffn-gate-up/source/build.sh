#!/usr/bin/env bash
set -euo pipefail
src=$(cd "$(dirname "$0")" && pwd)
prev="$src/../../EXP-2026-11-25-1998-bonsai-decay-reuse/source"
out=${1:?Specify a new scratch directory}
cuda=${CUDA_HOME:-/usr/local/cuda}
bash "$prev/build.sh" "$out"
cat "$out/decay-reuse.cl" > "$out/ffn-fused.cl"
python3 "$src/compose.py" >> "$out/ffn-fused.cl"
cat "$src/ffn-tail.cl" >> "$out/ffn-fused.cl"
clang-18 -target nvptx64-nvidia-cuda -march=sm_80 -cl-std=CL2.0 -x cl -O3 -ffp-contract=off \
   -nocudainc -nocudalib -Xclang -finclude-default-header -Xclang -mlink-builtin-bitcode \
   -Xclang "$cuda/nvvm/libdevice/libdevice.10.bc" -S "$out/ffn-fused.cl" -o "$out/ffn-fused.ptx"
"$cuda/bin/ptxas" -v -arch=sm_89 "$out/ffn-fused.ptx" -o "$out/ffn-fused.cubin" 2> "$out/ffn-fused.ptxas.log"
compiler=${GOOSE_BIN:-/home/end0/AI/Fried-Goose/releases/fried-goose-v0.3.0-preview-keelwing/bin/goose}
python3 "$src/compose-graph.py" "$out/bonsai.goose" > "$out/ffn-tail.goose"
"$compiler" --standalone -o "$out/ffn-tail.c" --header "$out/ffn-tail.h" "$out/ffn-tail.goose"
"${CC:-gcc-13}" -O2 -fPIC -ffunction-sections -fdata-sections -w -c "$out/ffn-tail.c" -o "$out/ffn-tail.o"
patch -s "$out/decay-backend.c" -o "$out/ffn-tail-backend.c" < "$src/backend.c.patch"
"${CC:-gcc-13}" -O2 -Wall -Wextra -Werror -Wl,--gc-sections -I"$out" -I"$cuda/include" \
    "$out/ffn-tail-backend.c" "$out/ffn-tail.o" \
    "$src/../../EXP-2026-11-25-1973-bonsai-pq2-bootstrap/source/bits.c" \
    -l:libcuda.so.1 -lm -pthread -o "$out/bonsai-ffn-tail"
