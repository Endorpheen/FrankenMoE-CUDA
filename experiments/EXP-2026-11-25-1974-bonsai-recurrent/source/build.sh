#!/usr/bin/env bash
set -euo pipefail
source_dir=$(cd "$(dirname "$0")" && pwd)
previous="$source_dir/../../EXP-2026-11-25-1973-bonsai-pq2-bootstrap/source"
cuda=${CUDA_HOME:-/usr/local/cuda}
out=${1:?Specify a new scratch directory}
mkdir "$out"
cat "$previous/bonsai-pq2.cl" "$source_dir/bonsai-recurrent.cl" > "$out/recurrent.cl"
clang-18 -target nvptx64-nvidia-cuda -march=sm_80 -cl-std=CL2.0 -x cl -O3 -ffp-contract=off \
    -nocudainc -nocudalib -Xclang -finclude-default-header -Xclang -mlink-builtin-bitcode \
    -Xclang "$cuda/nvvm/libdevice/libdevice.10.bc" -S "$out/recurrent.cl" -o "$out/recurrent.ptx"
"$cuda/bin/ptxas" -v -arch=sm_89 "$out/recurrent.ptx" -o "$out/recurrent.cubin" 2> "$out/ptxas.log"
