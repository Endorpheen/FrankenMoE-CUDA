#!/usr/bin/env bash
set -euo pipefail
source_dir=$(cd "$(dirname "$0")" && pwd)
parent="$source_dir/../../EXP-2026-11-25-1957-inkling-attention-bridge-microprobe/source"
out=${1:?Specify a new scratch directory}
cuda=${CUDA_HOME:-/usr/local/cuda}

bash "$parent/build.sh" "$out"
patch -s "$parent/attention-bridge.cl" -o "$out/attention-bridge.cl" < "$source_dir/attention-bridge.cl.patch"
cp "$source_dir/depth-expf.h" "$out/"
clang-18 -target nvptx64-nvidia-cuda -march=sm_80 -cl-std=CL2.0 -x cl -O3 -ffp-contract=off \
    -nocudainc -nocudalib -Xclang -finclude-default-header -DI2_EXPF_TEST \
    -S "$out/attention-bridge.cl" -o "$out/attention-bridge.ptx"
"$cuda/bin/ptxas" -v -arch=sm_89 "$out/attention-bridge.ptx" -o "$out/attention-bridge.cubin"
gcc-13 -O3 -Wall -Wextra -Werror -fno-builtin-expf -I "$cuda/include" \
    "$source_dir/audit-expf.c" -lcuda -lm -o "$out/audit-expf"
