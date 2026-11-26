#!/usr/bin/env bash
set -euo pipefail

src=$(cd "$(dirname "$0")" && pwd)
exp=$(cd "$src/../.." && pwd)
archive=${1:?Specify the original EXP-2047 mvq.cl}
out=${2:?Specify an isolated output directory}
cuda=${CUDA_HOME:-/usr/local/cuda}
mkdir -p "$out"
python3 "$src/emit.py" "$archive" "$exp/EXP-2026-11-25-2032-bonsai-mtp-batch-prefill/source/kv-batch.cl" "$out/q4.cl"
clang-18 -target nvptx64-nvidia-cuda -march=sm_80 -cl-std=CL2.0 -x cl -O3 -ffp-contract=off \
    -nocudainc -nocudalib -Xclang -finclude-default-header -S "$out/q4.cl" -o "$out/q4.ptx"
"$cuda/bin/ptxas" -v -arch=sm_89 "$out/q4.ptx" -o "$out/q4.cubin"
