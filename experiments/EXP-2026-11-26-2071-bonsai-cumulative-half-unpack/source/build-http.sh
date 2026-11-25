#!/usr/bin/env bash
set -euo pipefail

src=$(cd "$(dirname "$0")" && pwd)
root=$(git -C "$src" rev-parse --show-toplevel)
out=${1:?Specify the existing component build directory}
base=${2:?Specify EXP-2070 build directory}
build=${FRANKENMOE_CHECKPOINTS:-$root/build}
cuda=${CUDA_HOME:-/usr/local/cuda}
session="$root/experiments/EXP-2026-11-25-2002-bonsai-session/source"
host="$build/bonsai-mmq-k64-2000-default"
http="$build/bonsai-http-2003-link"
apex="$build/checkpoint-apex-exp1880-before-inkling"
inkling="$build/checkpoint-inkling-exp1969-before-bonsai"
device="$build/inkling-exp-baseline/device-attention"
archive="$build/bonsai-dp4a-2012-default/libfranken-inference.a"

python3 "$src/prepare-host.py" "$base/backend.c" "$out/backend.c"
gcc-13 -O2 -Wall -Wextra -Werror -ffunction-sections -fdata-sections \
    -I"$host" -I"$session" -I"$cuda/include" -Dmain=bonsai_diagnostic_main \
    -c "$out/backend.c" -o "$out/session-unlinked.o"
objcopy --redefine-syms="$http/rename.txt" "$out/session-unlinked.o" "$out/bonsai-session.o"
gcc-13 -O2 -std=gnu11 -D_GNU_SOURCE -DI2_NUMERIC_MODULE -Werror=implicit-function-declaration \
    -ffunction-sections -fdata-sections -I"$apex/build-inputs" -I"$apex/checkpoint" \
    -I"$root/vendor/flint/ggml/include" -I"$root/vendor/flint/ggml/src" \
    -I"$root/integration/goose-standalone" -I"$root/integration/m1/host" \
    -Wl,--gc-sections "$inkling/source/infer-main.c" \
    "$device/inkling-moe-tile32/numeric.o" "$out/bonsai-session.o" \
    "$http/bonsai-goose.o" "$archive" -lm -ldl -pthread -o "$out/frankenmoe-infer"
