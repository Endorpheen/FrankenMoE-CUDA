#!/usr/bin/env bash
set -euo pipefail
src=$(cd "$(dirname "$0")" && pwd)
out=${1:?Specify a new scratch directory}
base=${2:-build/bonsai-decode-decay-2017}
cuda=${CUDA_HOME:-/usr/local/cuda}
mkdir "$out"
cat "$base/decay.cl" > "$out/panels.cl"
python3 "$src/emit.py" "$base/decay.cl" >> "$out/panels.cl"
clang-18 -target nvptx64-nvidia-cuda -march=sm_80 -cl-std=CL2.0 -x cl -O3 -ffp-contract=off \
    -nocudainc -nocudalib -Xclang -finclude-default-header -Xclang -mlink-builtin-bitcode \
    -Xclang "$cuda/nvvm/libdevice/libdevice.10.bc" -S "$out/panels.cl" -o "$out/panels.ptx"
python3 "$src/dynamic.py" "$out/panels.ptx"
"$cuda/bin/ptxas" -v -arch=sm_89 "$out/panels.ptx" -o "$out/panels.cubin" 2>"$out/ptxas.txt"
