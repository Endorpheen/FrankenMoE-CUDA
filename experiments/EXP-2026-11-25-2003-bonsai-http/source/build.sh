#!/usr/bin/env bash
set -euo pipefail
src=$(cd "$(dirname "$0")" && pwd)
root=$(git rev-parse --show-toplevel)
checkpoint="$root/build/checkpoint-inkling-exp1969-before-bonsai"
apex="$root/build/checkpoint-apex-exp1880-before-inkling"
previous="$root/build/inkling-apex-gate/single-link-1953"
device="$root/build/inkling-exp-baseline/device-attention"
main=${FRANKEN_MAIN:-/home/end0/AI/FrankenMoE-CUDA-main}
bonsai="$root/build/bonsai-mmq-k64-2000-default"
session="$root/build/bonsai-session-2002"
out=${1:?Specify a new scratch directory}
mkdir "$out"
python3 "$src/compose.py" "$previous/model-host.c" "$out/model-host.c"
flags=(-O2 -std=gnu11 -D_GNU_SOURCE -DI2_NUMERIC_MODULE -Werror=implicit-function-declaration
    -ffunction-sections -fdata-sections -I "$checkpoint/source" -I "$apex/checkpoint" -I "$apex/build-inputs"
    -I "$previous" -I "$device/inkling-attention-device-input" -I "$device/inkling-completion" -I "$device/inkling-moe-tile32"
    -I "$src/../../EXP-2026-11-25-2002-bonsai-session/source"
    -I "$main/vendor/flint/ggml/include" -I "$main/vendor/flint/ggml/src"
    -I "$main/integration/goose-standalone" -I "$main/integration/m1/host")
gcc-13 "${flags[@]}" -c "$out/model-host.c" -o "$out/model-host.o"
nm -u "$session/bonsai-session.o" | awk '$NF ~ /^(gr_|bonsai_(bind_layout|graph)$|bt_eog_ids$|goose_init$)/ {print $NF}' > "$out/exports.txt"
awk '{print $1, "bonsai_" $1}' "$out/exports.txt" > "$out/rename.txt"
objcopy --keep-global-symbols="$out/exports.txt" "$bonsai/ffn-tail.o" "$out/goose-visible.o"
objcopy --redefine-syms="$out/rename.txt" "$out/goose-visible.o" "$out/bonsai-goose.o"
objcopy --redefine-syms="$out/rename.txt" "$session/bonsai-session.o" "$out/bonsai-session.o"
cp "$checkpoint/libfranken-inference.a" "$out/libfranken-inference.a"
ar r "$out/libfranken-inference.a" "$out/model-host.o"
gcc-13 "${flags[@]}" -Wl,--gc-sections "$checkpoint/source/infer-main.c" \
    "$device/inkling-moe-tile32/numeric.o" "$out/bonsai-session.o" "$out/bonsai-goose.o" \
    "$out/libfranken-inference.a" -l:libcuda.so.1 -lm -ldl -pthread -o "$out/frankenmoe-infer"
