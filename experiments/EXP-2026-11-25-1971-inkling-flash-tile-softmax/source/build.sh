#!/usr/bin/env bash
set -euo pipefail
source_dir=$(cd "$(dirname "$0")" && pwd)
repo=$(git rev-parse --show-toplevel)
out=${1:?Specify a new scratch directory}
previous="$repo/build/inkling-apex-gate/q8-packed-input-1969"
flash="$repo/build/inkling-exp-baseline/device-attention/inkling-prefill-parallel-relative"
driver="$repo/build/inkling-exp-baseline/device-attention/inkling-moe-tile32/native-kv-driver.h"
cuda=${CUDA_HOME:-/usr/local/cuda}
mkdir "$out"
cp "$previous/"{inkling-backend.c,prefill.h,prefill-team.h,graph-execute.c,cuda-driver.c,cuda-driver.reference.merged.c,infer-main.c,bridge.h,bridge-driver.h,bridge-image.h,decode-lfu.h,lfu.goose,link.sh,inkling-matrix-driver.h,q8-packed-image.h} "$out/"
cp "$flash/"{flash-mma.cl,device-types.h,inkling-online-device.h,flash-native.cubin} "$out/"
cp "$source_dir/"{generate.py,bench-prefill.py} "$out/"
python3 "$out/generate.py"
clang-18 -target nvptx64-nvidia-cuda -march=sm_80 -cl-std=CL2.0 -x cl -O3 -ffp-contract=off \
    -nocudainc -nocudalib -Xclang -finclude-default-header -include "$out/device-types.h" \
    -Xclang -mlink-builtin-bitcode -Xclang "$cuda/nvvm/libdevice/libdevice.10.bc" \
    -S "$out/prefill-variants.cl" -o "$out/variants.ptx"
"$cuda/bin/ptxas" -v -arch=sm_89 --entry i2_flash_prefill_vector,i2_flash_prefill_prune,i2_flash_prefill_combined,i2_flash_prefill_half,i2_flash_prefill_all,i2_flash_prefill_softmax,i2_flash_prefill_all_softmax \
    "$out/variants.ptx" -o "$out/variants.cubin"
"$cuda/bin/ptxas" -v -arch=sm_89 --entry i2_flash_prefill_all_softmax \
    "$out/variants.ptx" -o "$out/flash-tiles.cubin"
(cd "$out" && xxd -i flash-tiles.cubin > flash-tiles-image.h)
patch -s "$driver" -o "$out/native-kv-driver.h" < "$source_dir/native-kv-driver.h.patch"
bash "$out/link.sh"
