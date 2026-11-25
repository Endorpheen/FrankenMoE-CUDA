#!/usr/bin/env bash
set -euo pipefail
src=$(cd "$(dirname "$0")" && pwd)
out=${1:?Specify a new scratch directory}
base=${2:?Specify the EXP-2006 build directory}
cuda=${CUDA_HOME:-/usr/local/cuda}
mkdir "$out"
cat "$base/rows.cl" "$src/merge.cl" > "$out/merge.cl"
python3 "$src/emit.py" "$base/rows.cl" >> "$out/merge.cl"
clang-18 -target nvptx64-nvidia-cuda -march=sm_80 -cl-std=CL2.0 -x cl -O3 -ffp-contract=off \
    -nocudainc -nocudalib -Xclang -finclude-default-header -Xclang -mlink-builtin-bitcode \
    -Xclang "$cuda/nvvm/libdevice/libdevice.10.bc" -S "$out/merge.cl" -o "$out/merge.ptx"
"$cuda/bin/ptxas" -v -arch=sm_89 "$out/merge.ptx" -o "$out/merge.cubin" 2>"$out/ptxas.txt"
