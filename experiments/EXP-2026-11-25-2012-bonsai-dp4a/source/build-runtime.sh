#!/usr/bin/env bash
set -euo pipefail
src=$(cd "$(dirname "$0")" && pwd)
out=${1:?Specify a new scratch directory}
dev=${2:?Specify the DP4A build directory}
root=$(git rev-parse --show-toplevel)
base="$root/build/bonsai-flash-q-reuse-2011-http"
host="$root/build/bonsai-mmq-k64-2000-default"
http="$root/build/bonsai-http-2003-link"
apex="$root/build/checkpoint-apex-exp1880-before-inkling"
inkling="$root/build/checkpoint-inkling-exp1969-before-bonsai"
session="$src/../../EXP-2026-11-25-2002-bonsai-session/source"
device="$root/build/inkling-exp-baseline/device-attention"
oldhost="$root/build/inkling-apex-gate/single-link-1953"
main=${FRANKEN_MAIN:-/home/end0/AI/FrankenMoE-CUDA-main}
cuda=${CUDA_HOME:-/usr/local/cuda}
mkdir "$out"
python3 "$src/compose.py" "$base/backend.c" "$out/http-session.c" --http
python3 "$src/http-host.py" "$http/model-host.c" "$out/model-host.c"
gcc-13 -O2 -Wall -Wextra -Werror -ffunction-sections -fdata-sections \
    -I"$host" -I"$session" -I"$cuda/include" -Dmain=bonsai_diagnostic_main \
    -c "$out/http-session.c" -o "$out/session-unlinked.o"
objcopy --redefine-syms="$http/rename.txt" "$out/session-unlinked.o" "$out/bonsai-session.o"
flags=(-O2 -std=gnu11 -D_GNU_SOURCE -DI2_NUMERIC_MODULE -Werror=implicit-function-declaration
    -ffunction-sections -fdata-sections -I"$apex/build-inputs" -I"$apex/checkpoint"
    -I"$main/vendor/flint/ggml/include" -I"$main/vendor/flint/ggml/src"
    -I"$main/integration/goose-standalone" -I"$main/integration/m1/host"
    -I"$oldhost" -I"$device/inkling-attention-device-input" -I"$session")
gcc-13 "${flags[@]}" -c "$out/model-host.c" -o "$out/model-host.o"
cp "$http/libfranken-inference.a" "$out/libfranken-inference.a"
ar r "$out/libfranken-inference.a" "$out/model-host.o"
gcc-13 "${flags[@]}" -Wl,--gc-sections "$inkling/source/infer-main.c" \
    "$device/inkling-moe-tile32/numeric.o" "$out/bonsai-session.o" \
    "$http/bonsai-goose.o" "$out/libfranken-inference.a" -l:libcuda.so.1 -lm -ldl -pthread \
    -o "$out/frankenmoe-infer"
ln -s "$(realpath "$dev/mvq.cubin")" "$out/mvq.cubin"
