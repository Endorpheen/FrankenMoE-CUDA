#!/usr/bin/env bash
set -euo pipefail
src=$(cd "$(dirname "$0")" && pwd)
out=${1:?Specify a new scratch directory}
base=${2:-build/bonsai-ffn-q8-prefill-2014-scoped}
cuda=${CUDA_HOME:-/usr/local/cuda}
mkdir "$out"
cat "$base/ffn-q8.cl" > "$out/lookahead.cl"
python3 "$src/emit.py" "$base/ffn-q8.cl" >> "$out/lookahead.cl"
clang-18 -target nvptx64-nvidia-cuda -march=sm_80 -cl-std=CL2.0 -x cl -O3 -ffp-contract=off \
    -nocudainc -nocudalib -Xclang -finclude-default-header -Xclang -mlink-builtin-bitcode \
    -Xclang "$cuda/nvvm/libdevice/libdevice.10.bc" -S "$out/lookahead.cl" -o "$out/lookahead.ptx"
"$cuda/bin/ptxas" -v -arch=sm_89 "$out/lookahead.ptx" -o "$out/lookahead.cubin" 2>"$out/ptxas.txt"
