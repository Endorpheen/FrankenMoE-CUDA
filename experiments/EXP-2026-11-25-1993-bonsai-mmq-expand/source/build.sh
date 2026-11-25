#!/usr/bin/env bash
set -euo pipefail
src=$(cd "$(dirname "$0")" && pwd)
prev="$src/../../EXP-2026-11-25-1992-bonsai-mmq-wide/source"
out=${1:?Specify a new scratch directory}
cuda=${CUDA_HOME:-/usr/local/cuda}
bash "$prev/build.sh" "$out"
cat "$out/pq2-mmq-wide.cl" > "$out/pq2-expanded.cl"
python3 "$src/compose.py" "$out/pq2-mmq-wide.cl" >> "$out/pq2-expanded.cl"
clang-18 -target nvptx64-nvidia-cuda -march=sm_80 -cl-std=CL2.0 -x cl -O3 -ffp-contract=off \
   -nocudainc -nocudalib -Xclang -finclude-default-header -Xclang -mlink-builtin-bitcode \
   -Xclang "$cuda/nvvm/libdevice/libdevice.10.bc" -S "$out/pq2-expanded.cl" -o "$out/pq2-expanded.ptx"
"$cuda/bin/ptxas" -v -arch=sm_89 "$out/pq2-expanded.ptx" -o "$out/pq2-expanded.cubin" 2> "$out/pq2-expanded.ptxas.log"
