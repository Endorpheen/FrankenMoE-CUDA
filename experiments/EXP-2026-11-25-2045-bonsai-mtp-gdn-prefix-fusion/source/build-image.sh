#!/usr/bin/env bash
set -euo pipefail
src=$(cd "$(dirname "$0")" && pwd)
root=$(git rev-parse --show-toplevel)
out=${1:?Specify a diagnostic output directory}
base="$root/build/bonsai-ffn-prefix-2044-both"
exp="$src/../.."
cuda=${CUDA_HOME:-/usr/local/cuda}
mkdir -p "$out"
cat "$base/prefix.cl" "$src/verify-gdn.cl" > "$out/gdn.cl"
clang-18 -target nvptx64-nvidia-cuda -march=sm_80 -cl-std=CL2.0 -x cl -O3 -ffp-contract=off \
    -nocudainc -nocudalib -Xclang -finclude-default-header -Xclang -mlink-builtin-bitcode \
    -Xclang "$cuda/nvvm/libdevice/libdevice.10.bc" -S "$out/gdn.cl" -o "$out/gdn.ptx"
python3 "$exp/EXP-2026-11-25-2018-bonsai-flash-kv-panels/source/dynamic.py" "$out/gdn.ptx"
python3 "$exp/EXP-2026-11-25-2041-bonsai-flash-query-pack/source/dynamic.py" "$out/gdn.ptx"
"$cuda/bin/ptxas" -v -arch=sm_89 "$out/gdn.ptx" -o "$out/gdn.cubin" 2>"$out/ptxas.txt"
