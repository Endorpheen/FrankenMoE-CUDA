#!/usr/bin/env bash
set -euo pipefail
d=$(cd "$(dirname "$0")" && pwd)
repo=$(git rev-parse --show-toplevel)
apex="$repo/build/checkpoint-apex-exp1880-before-inkling"
p="$repo/build/inkling-exp-baseline/device-attention"
i="$p/inkling-moe-tile32"
main=/home/end0/AI/FrankenMoE-CUDA-main
previous="$repo/build/inkling-apex-gate/single-link-1953"
flags=(-O2 -std=gnu11 -D_GNU_SOURCE -DI2_NUMERIC_MODULE -Werror=implicit-function-declaration
 -ffunction-sections -fdata-sections -I "$d" -I "$apex/checkpoint" -I "$apex/build-inputs"
 -I "$i" -I "$p/inkling-grouped-moe-mmq"
 -I "$p/inkling-resident-moe" -I "$p/inkling-attention-device-input" -I "$p/inkling-q4-kv"
 -I "$p/inkling-cuda-tanh" -I "$p/inkling-completion" -I "$p/inkling-fused-situ"
 -I "$p/../slru-prototype" -I "$p/flash-prefill/native-runtime"
 -I "$main/vendor/flint/ggml/include" -I "$main/vendor/flint/ggml/src"
 -I "$main/integration/goose-standalone" -I "$main/integration/m1/host")
macro_map=()
if [[ -n ${ASSERT_SOURCE_DIR:-} ]]; then
    macro_map+=("-fmacro-prefix-map=$d=$ASSERT_SOURCE_DIR")
fi
# The Inkling backend must see its generated model ABI first.
gcc-13 "${macro_map[@]}" -I "$p/inkling-completion" -I "$p/inkling-pinned-activation-readback" "${flags[@]}" -c "$d/inkling-backend.c" -o "$d/inkling-backend.o"
gcc-13 "${flags[@]}" -c "$d/graph-execute.c" -o "$d/graph-execute.o"
gcc-13 "${flags[@]}" -c "$d/cuda-driver.c" -o "$d/cuda-driver.o"
cp "$previous/libfranken-inference.a" "$d/libfranken-inference.a"
ar r "$d/libfranken-inference.a" "$d/"{cuda-driver,inkling-backend,graph-execute}.o
gcc-13 "${flags[@]}" -Wl,--gc-sections "$d/infer-main.c" "$i/numeric.o" \
    "$d/libfranken-inference.a" -lm -ldl -pthread -o "$d/frankenmoe-infer"
