#!/usr/bin/env bash
set -euo pipefail
src=$(cd "$(dirname "$0")" && pwd)
out=${1:?Specify a new scratch directory}
base=${2:-build/bonsai-mmq-pipeline-2019-packed}
cuda=${CUDA_HOME:-/usr/local/cuda}
mkdir "$out"
cat "$base/pipeline.cl" "$src/unpack.cl" > "$out/half2.cl"
python3 "$src/emit.py" "$base/pipeline.cl" >> "$out/half2.cl"
clang-18 -target nvptx64-nvidia-cuda -march=sm_80 -cl-std=CL2.0 -x cl -O3 -ffp-contract=off \
    -nocudainc -nocudalib -Xclang -finclude-default-header -Xclang -mlink-builtin-bitcode \
    -Xclang "$cuda/nvvm/libdevice/libdevice.10.bc" -S "$out/half2.cl" -o "$out/half2.ptx"
python3 "$src/../../EXP-2026-11-25-2018-bonsai-flash-kv-panels/source/dynamic.py" "$out/half2.ptx"
"$cuda/bin/ptxas" -v -arch=sm_89 "$out/half2.ptx" -o "$out/half2.cubin" 2>"$out/ptxas.txt"
