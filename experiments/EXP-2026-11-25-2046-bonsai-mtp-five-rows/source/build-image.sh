#!/usr/bin/env bash
set -euo pipefail
src=$(cd "$(dirname "$0")" && pwd)
root=$(git rev-parse --show-toplevel)
out=${1:?Specify a diagnostic output directory}
base="$root/build/bonsai-ffn-prefix-2044-both"
softmax="$root/build/bonsai-mtp-softmax-2042"
exp="$src/../.."
cuda=${CUDA_HOME:-/usr/local/cuda}
mkdir -p "$out"
python3 "$src/emit.py" \
    "$exp/EXP-2026-11-25-2028-bonsai-mtp-q8-batch/source/verify-mvq.cl" "$softmax/variants.cl" \
    "$exp/EXP-2026-11-25-2039-bonsai-mtp-three-proposals/source/conv-four.cl" "$out/five.cl"
cat "$base/prefix.cl" "$out/five.cl" > "$out/verify.cl"
clang-18 -target nvptx64-nvidia-cuda -march=sm_80 -cl-std=CL2.0 -x cl -O3 -ffp-contract=off \
    -nocudainc -nocudalib -Xclang -finclude-default-header -Xclang -mlink-builtin-bitcode \
    -Xclang "$cuda/nvvm/libdevice/libdevice.10.bc" -S "$out/verify.cl" -o "$out/verify.ptx"
python3 "$exp/EXP-2026-11-25-2018-bonsai-flash-kv-panels/source/dynamic.py" "$out/verify.ptx"
python3 "$exp/EXP-2026-11-25-2041-bonsai-flash-query-pack/source/dynamic.py" "$out/verify.ptx"
"$cuda/bin/ptxas" -v -arch=sm_89 "$out/verify.ptx" -o "$out/verify.cubin" 2>"$out/ptxas.txt"
