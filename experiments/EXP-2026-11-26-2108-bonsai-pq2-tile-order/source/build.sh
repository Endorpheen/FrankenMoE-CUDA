#!/usr/bin/env bash
set -euo pipefail

src=$(cd "$(dirname "$0")" && pwd)
reference=${1:?Specify the EXP-2086 component OpenCL source}
out=${2:?Specify an isolated output directory}
cuda=${CUDA_HOME:-/usr/local/cuda}
mkdir -p "$out"
printf '1f635185b260ff137ec7e57d826140ae8c3893452074a8f68d5c5c4ca9f2381a  %s\n' "$reference" | sha256sum --strict -c -
python3 "$src/emit.py" "$reference" "$out/component.cl"
clang-18 -target nvptx64-nvidia-cuda -march=sm_80 -cl-std=CL2.0 -x cl -O3 -ffp-contract=off \
    -nocudainc -nocudalib -Xclang -finclude-default-header -Xclang -mlink-builtin-bitcode \
    -Xclang "$cuda/nvvm/libdevice/libdevice.10.bc" -S "$out/component.cl" -o "$out/component.ptx"
python3 "$src/emit.py" --extract "$out/component.ptx" "$out/order.ptx"
"$cuda/bin/ptxas" -v -arch=sm_89 "$out/order.ptx" -o "$out/order.cubin"
