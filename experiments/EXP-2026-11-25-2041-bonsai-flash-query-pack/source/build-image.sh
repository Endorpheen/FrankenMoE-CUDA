#!/usr/bin/env bash
set -euo pipefail
src=$(cd "$(dirname "$0")" && pwd)
root=$(git rev-parse --show-toplevel)
out=${1:?Specify a diagnostic output directory}
base="$root/build/bonsai-mtp-merge-2036"
cuda=${CUDA_HOME:-/usr/local/cuda}
mkdir -p "$out"
python3 "$src/emit.py" "$base/merge.cl" "$out/variants.cl"
cat "$base/merge.cl" "$out/variants.cl" "$src/pack-panels.cl" > "$out/flash.cl"
clang-18 -target nvptx64-nvidia-cuda -march=sm_80 -cl-std=CL2.0 -x cl -O3 -ffp-contract=off \
    -nocudainc -nocudalib -Xclang -finclude-default-header -Xclang -mlink-builtin-bitcode \
    -Xclang "$cuda/nvvm/libdevice/libdevice.10.bc" -S "$out/flash.cl" -o "$out/flash.ptx"
python3 "$src/../../EXP-2026-11-25-2018-bonsai-flash-kv-panels/source/dynamic.py" "$out/flash.ptx"
python3 "$src/dynamic.py" "$out/flash.ptx"
"$cuda/bin/ptxas" -v -arch=sm_89 "$out/flash.ptx" -o "$out/flash.cubin" 2>"$out/ptxas.txt"
