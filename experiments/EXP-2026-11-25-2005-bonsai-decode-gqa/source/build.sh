#!/usr/bin/env bash
set -euo pipefail
src=$(cd "$(dirname "$0")" && pwd)
out=${1:?Specify a new scratch directory}
cuda=${CUDA_HOME:-/usr/local/cuda}
mkdir "$out"
clang-18 -target nvptx64-nvidia-cuda -march=sm_80 -cl-std=CL2.0 -x cl -O3 -ffp-contract=off \
    -nocudainc -nocudalib -Xclang -finclude-default-header -Xclang -mlink-builtin-bitcode \
    -Xclang "$cuda/nvvm/libdevice/libdevice.10.bc" -S "$src/grouped.cl" -o "$out/grouped.ptx"
"$cuda/bin/ptxas" -v -arch=sm_89 "$out/grouped.ptx" -o "$out/grouped.cubin" 2>"$out/ptxas.txt"
