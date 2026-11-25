#!/usr/bin/env bash
set -euo pipefail
source_dir=$(cd "$(dirname "$0")" && pwd)
repo=$(git rev-parse --show-toplevel)
out=${1:?Specify a new scratch directory}
previous="$repo/build/inkling-apex-gate/parallel-logits-1966"
matrix="$repo/build/inkling-exp-baseline/device-attention/inkling-iq4-xs-block-decode/matrix.cubin"
driver="$repo/build/inkling-exp-baseline/device-attention/inkling-moe-tile32/inkling-matrix-driver.h"
cuda=${CUDA_HOME:-/usr/local/cuda}
mkdir "$out"
cp "$previous/"{inkling-backend.c,prefill.h,prefill-team.h,graph-execute.c,cuda-driver.c,cuda-driver.reference.merged.c,infer-main.c,bridge.h,bridge-driver.h,bridge-image.h,decode-lfu.h,lfu.goose,link.sh} "$out/"
cp "$source_dir/"{q8-packed.cl,bench-packed.py} "$out/"
cp "$matrix" "$out/control.cubin"
clang-18 -target nvptx64-nvidia-cuda -march=sm_80 -cl-std=CL2.0 -x cl -O3 -ffp-contract=off \
  -nocudainc -nocudalib -Xclang -finclude-default-header -S "$out/q8-packed.cl" -o "$out/q8-packed.ptx"
"$cuda/bin/ptxas" -v -arch=sm_89 "$out/q8-packed.ptx" -o "$out/q8-packed.cubin"
(cd "$out" && xxd -i q8-packed.cubin > q8-packed-image.h)
patch -s "$driver" -o "$out/inkling-matrix-driver.h" < "$source_dir/inkling-matrix-driver.h.patch"
bash "$out/link.sh"
