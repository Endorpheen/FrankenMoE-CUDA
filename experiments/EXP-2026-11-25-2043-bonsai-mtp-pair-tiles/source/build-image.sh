#!/usr/bin/env bash
set -euo pipefail
src=$(cd "$(dirname "$0")" && pwd)
root=$(git rev-parse --show-toplevel)
out=${1:?Specify a diagnostic output directory}
base="$root/build/bonsai-mtp-softmax-2042"
width="$root/build/bonsai-mtp-width-2039"
exp="$src/../.."
cuda=${CUDA_HOME:-/usr/local/cuda}
mkdir -p "$out"
python3 "$src/emit.py" "$exp/EXP-2026-11-25-2033-bonsai-mtp-attention-reuse/source/verify-attention.cl" "$out/pairs.cl"
cat "$base/softmax.cl" "$width/verify-four.cl" "$out/pairs.cl" > "$out/attention.cl"
clang-18 -target nvptx64-nvidia-cuda -march=sm_80 -cl-std=CL2.0 -x cl -O3 -ffp-contract=off \
    -nocudainc -nocudalib -Xclang -finclude-default-header -Xclang -mlink-builtin-bitcode \
    -Xclang "$cuda/nvvm/libdevice/libdevice.10.bc" -S "$out/attention.cl" -o "$out/attention.ptx"
python3 "$exp/EXP-2026-11-25-2018-bonsai-flash-kv-panels/source/dynamic.py" "$out/attention.ptx"
python3 "$exp/EXP-2026-11-25-2041-bonsai-flash-query-pack/source/dynamic.py" "$out/attention.ptx"
"$cuda/bin/ptxas" -v -arch=sm_89 "$out/attention.ptx" -o "$out/attention.cubin" 2>"$out/ptxas.txt"
