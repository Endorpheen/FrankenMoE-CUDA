#!/usr/bin/env bash
set -euo pipefail
src=$(cd "$(dirname "$0")" && pwd)
out=${1:?Specify a new scratch directory}
base=${2:-build/bonsai-flash-q-reuse-2011-http}
cuda=${CUDA_HOME:-/usr/local/cuda}
mkdir "$out"
cat "$base/direct.cl" "$src/dp4a.cl" > "$out/mvq.cl"
clang-18 -target nvptx64-nvidia-cuda -march=sm_80 -cl-std=CL2.0 -x cl -O3 -ffp-contract=off \
    -nocudainc -nocudalib -Xclang -finclude-default-header -Xclang -mlink-builtin-bitcode \
    -Xclang "$cuda/nvvm/libdevice/libdevice.10.bc" -S "$out/mvq.cl" -o "$out/mvq.ptx"
"$cuda/bin/ptxas" -v -arch=sm_89 "$out/mvq.ptx" -o "$out/mvq.cubin" 2>"$out/ptxas.txt"
