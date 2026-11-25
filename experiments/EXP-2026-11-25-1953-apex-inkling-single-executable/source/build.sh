#!/usr/bin/env bash
set -euo pipefail
repo=$(git rev-parse --show-toplevel)
source_dir=$(cd "$(dirname "$0")" && pwd)
out=${1:?Specify a new scratch directory}
apex_sources=${APEX_SOURCES:-/home/end0/AI/claude-apex-100/release/merge}
apex_cpu=${APEX_CPU:-/home/end0/AI/claude-apex-100/checkpoints/codex-20261124-moe-epilogue-accepted}
rebased=${REBASED_DRIVER:-"$repo/build/inkling-apex-gate/driver-rebase"}
main=${FRANKEN_MAIN:-/home/end0/AI/FrankenMoE-CUDA-main}
goose=${GOOSE_BIN:-/home/end0/AI/Fried-Goose/releases/fried-goose-v0.3.0-preview-keelwing/bin/goose}
apex="$repo/build/checkpoint-apex-exp1880-before-inkling"
device="$repo/build/inkling-exp-baseline/device-attention"
inkling="$device/inkling-moe-tile32"
mkdir "$out"
out=$(realpath "$out")

apply_source() {
    local name=$1 base=$2
    patch -s "$base" -o "$out/$name" < "$source_dir/$name.patch"
}
apply_source model-host.c "$apex_sources/goose-host-llm.speed.c"
apply_source graph-execute.c "$apex_sources/graph-execute.speed.c"
apply_source infer-main.c "$apex/build-inputs/infer-main.c"
apply_source cpu.goose "$apex_cpu/cpu.goose"
apply_source cuda-driver.reference.merged.c "$rebased/cuda-driver.reference.merged.c"
cp "$source_dir/apex-cached-decode.h" "$rebased/cuda-driver.c" "$out/"
cp -r "$apex_cpu/codec" "$out/codec"

"$goose" -o "$out/cpu.c" --header "$out/cpu-generated.h" "$out/cpu.goose"
gcc-13 -O1 -std=c11 -ffunction-sections -fdata-sections -Dgoose_init=goose_cpu_init \
    -Dgg_init_globals=goose_cpu_globals -w -c "$out/cpu.c" -o "$out/cpu-unlinked.o"
objcopy --redefine-sym goose_cpu_init=fg143_cpu_init \
    --redefine-sym goose_cpu_globals=fg143_cpu_init_globals "$out/cpu-unlinked.o" "$out/cpu.o"

flags=(-O2 -std=gnu11 -D_GNU_SOURCE -Werror=implicit-function-declaration
    -ffunction-sections -fdata-sections -I "$apex/checkpoint" -I "$apex/build-inputs"
    -I "$inkling" -I "$device/inkling-grouped-moe-mmq" -I "$device/inkling-resident-moe"
    -I "$device/inkling-attention-device-input" -I "$device/inkling-q4-kv"
    -I "$device/inkling-cuda-tanh" -I "$device/inkling-completion" -I "$device/inkling-fused-situ"
    -I "$device/../slru-prototype" -I "$device/flash-prefill/native-runtime"
    -I "$main/vendor/flint/ggml/include" -I "$main/vendor/flint/ggml/src"
    -I "$main/integration/goose-standalone" -I "$main/integration/m1/host")
gcc-13 "${flags[@]}" -DI2_NUMERIC_MODULE -c "$out/cuda-driver.c" -o "$out/cuda-driver.o"
gcc-13 "${flags[@]}" -c "$out/model-host.c" -o "$out/model-host.o"
gcc-13 "${flags[@]}" -c "$out/graph-execute.c" -o "$out/graph-execute.o"

mkdir "$out/inkling"
(cd "$out/inkling" && ar x "$inkling/libfranken-inference.a" main.o prior.o bucket.o tokenizer.o inkling-backend.o)
nm -g --defined-only "$out/inkling/main.o" |
    awk '$NF ~ /^i2_/ || $NF == "goose_init" || $NF == "ec_prepare" { print $NF }' > "$out/inkling-exports.txt"
objcopy --keep-global-symbols="$out/inkling-exports.txt" \
    "$out/inkling/main.o" "$out/inkling-model-visible.o"
objcopy --redefine-sym goose_init=inkling_model_init --redefine-sym ec_prepare=inkling_ec_prepare \
    "$out/inkling-model-visible.o" "$out/inkling-model.o"

cp "$apex/checkpoint/lib.a" "$out/libfranken-inference.a"
ar r "$out/libfranken-inference.a" "$out/"{cpu,cuda-driver,graph-execute,model-host,inkling-model}.o \
    "$out/inkling/"{prior,bucket,tokenizer,inkling-backend}.o
gcc-13 "${flags[@]}" -DI2_NUMERIC_MODULE -Wl,--gc-sections "$out/infer-main.c" "$inkling/numeric.o" \
    "$out/libfranken-inference.a" -lm -ldl -pthread -o "$out/frankenmoe-infer"
