#!/usr/bin/env bash
set -euo pipefail
src=$(cd "$(dirname "$0")" && pwd)
root=$(git rev-parse --show-toplevel)
out=${1:?Specify a diagnostic output directory}
base="$root/build/bonsai-mtp-five-2046"
exp="$src/../.."
cuda=${CUDA_HOME:-/usr/local/cuda}
mkdir -p "$out"
python3 "$src/emit.py" "$exp/EXP-2026-11-25-2028-bonsai-mtp-q8-batch/source/verify-mvq.cl" "$out/shapes.cl"
cat "$base/verify.cl" "$out/shapes.cl" > "$out/mvq.cl"
clang-18 -target nvptx64-nvidia-cuda -march=sm_80 -cl-std=CL2.0 -x cl -O3 -ffp-contract=off \
    -nocudainc -nocudalib -Xclang -finclude-default-header -Xclang -mlink-builtin-bitcode \
    -Xclang "$cuda/nvvm/libdevice/libdevice.10.bc" -S "$out/mvq.cl" -o "$out/mvq.ptx"
python3 "$exp/EXP-2026-11-25-2018-bonsai-flash-kv-panels/source/dynamic.py" "$out/mvq.ptx"
python3 "$exp/EXP-2026-11-25-2041-bonsai-flash-query-pack/source/dynamic.py" "$out/mvq.ptx"
"$cuda/bin/ptxas" -v -arch=sm_89 "$out/mvq.ptx" -o "$out/mvq.cubin" 2>"$out/ptxas.txt"
