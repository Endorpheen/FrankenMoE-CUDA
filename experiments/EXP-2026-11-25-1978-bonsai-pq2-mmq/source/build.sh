#!/usr/bin/env bash
set -euo pipefail
src=$(cd "$(dirname "$0")" && pwd)
prev="$src/../../EXP-2026-11-25-1973-bonsai-pq2-bootstrap/source"
cuda=${CUDA_HOME:-/usr/local/cuda}
out=${1:?Specify a new scratch directory}
mkdir "$out"
cat "$prev/bonsai-pq2.cl" "$src/pq2-mmq.cl" > "$out/mmq.cl"
clang-18 -target nvptx64-nvidia-cuda -march=sm_80 -cl-std=CL2.0 -x cl -O3 -ffp-contract=off \
    -nocudainc -nocudalib -Xclang -finclude-default-header -Xclang -mlink-builtin-bitcode \
    -Xclang "$cuda/nvvm/libdevice/libdevice.10.bc" -S "$out/mmq.cl" -o "$out/mmq.ptx"
"$cuda/bin/ptxas" -v -arch=sm_89 "$out/mmq.ptx" -o "$out/mmq.cubin" 2> "$out/ptxas.log"
