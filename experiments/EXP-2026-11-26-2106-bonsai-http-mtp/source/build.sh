#!/usr/bin/env bash
set -euo pipefail

src=$(cd "$(dirname "$0")" && pwd)
root=$(git -C "$src" rev-parse --show-toplevel)
out=${1:?Specify an isolated output directory}
base=${2:?Specify EXP-2105 build directory}
main=${3:?Specify EXP-2101 main-scan.goose}
saved=${FRANKENMOE_CHECKPOINTS:?Specify the checkpoint input directory}
goose=${GOOSE_BIN:-/home/end0/AI/Fried-Goose/releases/fried-goose-v0.3.0-preview-keelwing/bin/goose}
prior="$root/experiments/EXP-2026-11-26-2105-bonsai-ffn-graph/source"
prefix="$saved/bonsai-ffn-prefix-2044-both/prefix.o"
mkdir -p "$out"
python3 "$src/prepare.py" "$base" "$main" "$saved" "$out"
objcopy --redefine-sym bonsai_graph=bonsai_mtp_target_graph --redefine-sym goose_init=bonsai_mtp_goose_init "$prefix" "$out/mtp-export.o"
objcopy --keep-global-symbol bonsai_mtp_target_graph --keep-global-symbol bonsai_mtp_head \
    --keep-global-symbol bonsai_mtp_goose_init --keep-global-symbol bonsai_mtp_kv --keep-global-symbol bonsai_mtp_kv_batch \
    "$out/mtp-export.o" "$out/mtp-goose.o"
"$goose" --ast-budget-mib 2048 --standalone -o "$out/main.c" --header "$out/main.h" "$out/main-mtp.goose"
gcc-13 -O1 -std=c11 -ffunction-sections -fdata-sections -w -c "$out/main.c" -o "$out/main.o"
python3 "$root/tools/validate-cold-32k.py" instrument-bonsai "$out/backend.c" "$out/backend-cold.c"
gcc-13 -O2 -Wall -Wextra -Werror -ffunction-sections -fdata-sections \
    -I"$src" -I"$prior" -I"$saved/bonsai-mmq-k64-2000-default" \
    -I"$root/experiments/EXP-2026-11-25-2002-bonsai-session/source" -I"${CUDA_HOME:-/usr/local/cuda}/include" \
    -Dmain=bonsai_diagnostic_main -c "$out/backend-cold.c" -o "$out/session-unlinked.o"
objcopy --redefine-syms="$saved/bonsai-http-2003-link/rename.txt" \
    "$out/session-unlinked.o" "$out/bonsai-session.o"
apex="$saved/checkpoint-apex-exp1880-before-inkling"
inkling="$saved/checkpoint-inkling-exp1969-before-bonsai"
device="$saved/inkling-exp-baseline/device-attention"
flags=(-O2 -std=gnu11 -D_GNU_SOURCE -DI2_NUMERIC_MODULE -Werror=implicit-function-declaration
    -ffunction-sections -fdata-sections -I"$apex/build-inputs" -I"$apex/checkpoint"
    -I"$root/vendor/flint/ggml/include" -I"$root/vendor/flint/ggml/src"
    -I"$root/integration/goose-standalone" -I"$root/integration/m1/host"
    -I"$saved/inkling-apex-gate/single-link-1953" -I"$device/inkling-attention-device-input"
    -I"$root/experiments/EXP-2026-11-25-2002-bonsai-session/source")
gcc-13 "${flags[@]}" -c "$out/model-host.c" -o "$out/model-host.o"
cp "$base/libfranken-inference.a" "$out/libfranken-inference.a"
ar r "$out/libfranken-inference.a" "$out/main.o" "$out/model-host.o"
gcc-13 "${flags[@]}" -Wl,--gc-sections "$inkling/source/infer-main.c" \
    "$device/inkling-moe-tile32/numeric.o" "$out/bonsai-session.o" \
    "$saved/bonsai-http-2003-link/bonsai-goose.o" "$out/mtp-goose.o" "$out/libfranken-inference.a" \
    -lm -ldl -pthread -o "$out/frankenmoe-infer"
cp "$base/pipeline.cubin" "$out/pipeline.cubin"
cp "$saved/bonsai-mtp-warp-rows-2047/mvq.cubin" "$out/mtp.cubin"
