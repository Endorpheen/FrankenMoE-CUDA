#!/usr/bin/env bash
set -euo pipefail
worktree=$(git rev-parse --show-toplevel)
source_dir=$(cd "$(dirname "$0")" && pwd)
out=${1:?Specify a new scratch directory}
main=${2:-/home/end0/AI/FrankenMoE-CUDA-main}
mkdir "$out"
out=$(realpath "$out")
apex="$worktree/build/checkpoint-apex-exp1880-before-inkling"
device="$worktree/build/inkling-exp-baseline/device-attention"
inkling="$device/inkling-moe-tile32"

patch -s "$apex/checkpoint/cuda-driver.c" -o "$out/cuda-driver.reference.merged.c"     < "$source_dir/cuda-driver-reference.c.patch"
sed 's/"cuda-driver-reference.c"/"cuda-driver.reference.merged.c"/'     "$inkling/cuda-driver.c" > "$out/cuda-driver.c"
flags=(-O2 -std=gnu11 -D_GNU_SOURCE -DI2_NUMERIC_MODULE
    -Werror=implicit-function-declaration -ffunction-sections -fdata-sections
    -I "$apex/checkpoint" -I "$apex/build-inputs" -I "$inkling"
    -I "$device/inkling-grouped-moe-mmq" -I "$device/inkling-resident-moe"
    -I "$device/inkling-attention-device-input" -I "$device/inkling-q4-kv"
    -I "$device/inkling-cuda-tanh" -I "$device/inkling-completion"
    -I "$device/../slru-prototype" -I "$device/flash-prefill/native-runtime"
    -I "$main/vendor/flint/ggml/include" -I "$main/vendor/flint/ggml/src"
    -I "$main/integration/goose-standalone" -I "$main/integration/m1/host")
gcc-13 "${flags[@]}" -c "$out/cuda-driver.c" -o "$out/cuda-driver.o"

mkdir "$out/apex-objects" "$out/inkling-objects"
(cd "$out/apex-objects" && ar x "$apex/checkpoint/lib.a" main.o planner-link.o)
(cd "$out/inkling-objects" && ar x "$inkling/libfranken-inference.a" prior.o bucket.o)

cp "$apex/checkpoint/lib.a" "$out/apex-driver-probe.a"
ar r "$out/apex-driver-probe.a" "$out/cuda-driver.o" "$out/inkling-objects/"{prior,bucket}.o
gcc-13 "${flags[@]}" -Wl,--gc-sections "$apex/build-inputs/infer-main.c"     "$out/apex-driver-probe.a" -lm -ldl -pthread -o "$out/apex-layout-probe"

objcopy --keep-global-symbol=ec_prepare_compact     "$out/apex-objects/main.o" "$out/apex-compact.o"
cp "$inkling/libfranken-inference.a" "$out/inkling-main-probe.a"
ar r "$out/inkling-main-probe.a" "$out/cuda-driver.o" "$out/apex-compact.o"     "$out/apex-objects/planner-link.o"
gcc-13 "${flags[@]}" -Wl,--gc-sections "$device/inkling-completion/infer-main.c"     "$inkling/numeric.o" "$out/inkling-main-probe.a" -lm -ldl -pthread     -o "$out/inkling-layout-probe"
