#!/usr/bin/env bash
set -euo pipefail
src=$(cd "$(dirname "$0")" && pwd)
out=${1:?Specify a new scratch directory}
base=${2:-build/bonsai-row-reuse-2006-http}
cuda=${CUDA_HOME:-/usr/local/cuda}
prior="$src/../../EXP-2026-11-25-1994-bonsai-pq2-int8/source/pq2-int8.cl"
mkdir "$out"
cat "$base/rows.cl" "$prior" "$src/pack.cl" > "$out/d4.cl"
python3 "$src/../../EXP-2026-11-25-1996-bonsai-pq2-q8-residual/source/compose.py" "$prior" >> "$out/d4.cl"
python3 "$src/emit.py" "$prior" >> "$out/d4.cl"
clang-18 -target nvptx64-nvidia-cuda -march=sm_80 -cl-std=CL2.0 -x cl -O3 -ffp-contract=off \
    -nocudainc -nocudalib -Xclang -finclude-default-header -Xclang -mlink-builtin-bitcode \
    -Xclang "$cuda/nvvm/libdevice/libdevice.10.bc" -S "$out/d4.cl" -o "$out/d4.ptx"
"$cuda/bin/ptxas" -v -arch=sm_89 "$out/d4.ptx" -o "$out/d4.cubin" 2>"$out/ptxas.txt"
