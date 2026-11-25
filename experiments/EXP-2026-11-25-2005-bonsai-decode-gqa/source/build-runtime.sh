#!/usr/bin/env bash
set -euo pipefail
src=$(cd "$(dirname "$0")" && pwd)
root=$(git rev-parse --show-toplevel)
out=${1:?Specify a new scratch directory}
cuda=${CUDA_HOME:-/usr/local/cuda}
base="$root/build/bonsai-mmq-k64-2000-default"
http="$root/build/bonsai-http-2003-link"
apex="$root/build/checkpoint-apex-exp1880-before-inkling"
inkling="$root/build/checkpoint-inkling-exp1969-before-bonsai"
session="$src/../../EXP-2026-11-25-2002-bonsai-session/source"
main=${FRANKEN_MAIN:-/home/end0/AI/FrankenMoE-CUDA-main}
mkdir "$out"
cat "$base/mmq-k64.cl" > "$out/combined.cl"
sed 's/warp_sum(/bonsai_decode_warp_sum(/g' "$src/grouped.cl" >> "$out/combined.cl"
clang-18 -target nvptx64-nvidia-cuda -march=sm_80 -cl-std=CL2.0 -x cl -O3 -ffp-contract=off \
    -nocudainc -nocudalib -Xclang -finclude-default-header -Xclang -mlink-builtin-bitcode \
    -Xclang "$cuda/nvvm/libdevice/libdevice.10.bc" -S "$out/combined.cl" -o "$out/combined.ptx"
"$cuda/bin/ptxas" -v -arch=sm_89 "$out/combined.ptx" -o "$out/combined.cubin" 2>"$out/ptxas.txt"
python3 "$src/compose.py" "$base/mmq-k64-backend.c" "$session/session.inc" "$out/backend.c"
gcc-13 -O2 -Wall -Wextra -Werror -ffunction-sections -fdata-sections \
    -I"$base" -I"$session" -I"$cuda/include" -Dmain=bonsai_diagnostic_main \
    -c "$out/backend.c" -o "$out/session-unlinked.o"
objcopy --redefine-syms="$http/rename.txt" "$out/session-unlinked.o" "$out/bonsai-session.o"
gcc-13 -O2 -std=gnu11 -D_GNU_SOURCE -DI2_NUMERIC_MODULE -Werror=implicit-function-declaration \
    -ffunction-sections -fdata-sections -I"$apex/build-inputs" -I"$apex/checkpoint" \
    -I"$main/vendor/flint/ggml/include" -I"$main/vendor/flint/ggml/src" \
    -I"$main/integration/goose-standalone" -I"$main/integration/m1/host" \
    -Wl,--gc-sections "$inkling/source/infer-main.c" \
    "$root/build/inkling-exp-baseline/device-attention/inkling-moe-tile32/numeric.o" \
    "$out/bonsai-session.o" "$http/bonsai-goose.o" "$http/libfranken-inference.a" \
    -l:libcuda.so.1 -lm -ldl -pthread -o "$out/frankenmoe-infer"
