#!/usr/bin/env bash
set -euo pipefail
src=$(cd "$(dirname "$0")" && pwd)
out=${1:?Specify a new scratch directory}
base=${2:-build/bonsai-row-reuse-2006-http}
cuda=${CUDA_HOME:-/usr/local/cuda}
mkdir "$out"
cat "$base/rows.cl" > "$out/direct.cl"
python3 "$src/emit.py" "$base/rows.cl" >> "$out/direct.cl"
clang-18 -target nvptx64-nvidia-cuda -march=sm_80 -cl-std=CL2.0 -x cl -O3 -ffp-contract=off \
    -nocudainc -nocudalib -Xclang -finclude-default-header -Xclang -mlink-builtin-bitcode \
    -Xclang "$cuda/nvvm/libdevice/libdevice.10.bc" -S "$out/direct.cl" -o "$out/direct.ptx"
"$cuda/bin/ptxas" -v -arch=sm_89 "$out/direct.ptx" -o "$out/direct.cubin" 2>"$out/ptxas.txt"
