#!/usr/bin/env bash
set -euo pipefail
source_dir=$(cd "$(dirname "$0")" && pwd)
experiments=$(cd "$source_dir/../.." && pwd)
cuda=${CUDA_HOME:-/usr/local/cuda}
out=${1:?Specify a new scratch directory}
mkdir "$out"
cat "$experiments/EXP-2026-11-25-1973-bonsai-pq2-bootstrap/source/bonsai-pq2.cl" \
    "$experiments/EXP-2026-11-25-1974-bonsai-recurrent/source/bonsai-recurrent.cl" \
    "$source_dir/bonsai-attention.cl" > "$out/attention.cl"
clang-18 -target nvptx64-nvidia-cuda -march=sm_80 -cl-std=CL2.0 -x cl -O3 -ffp-contract=off \
    -nocudainc -nocudalib -Xclang -finclude-default-header -Xclang -mlink-builtin-bitcode \
    -Xclang "$cuda/nvvm/libdevice/libdevice.10.bc" -S "$out/attention.cl" -o "$out/attention.ptx"
"$cuda/bin/ptxas" -v -arch=sm_89 "$out/attention.ptx" -o "$out/attention.cubin" 2> "$out/ptxas.log"
