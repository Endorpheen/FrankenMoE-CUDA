#!/usr/bin/env bash
set -euo pipefail

src=$(cd "$(dirname "$0")" && pwd)
root=$(git -C "$src" rev-parse --show-toplevel)
out=${1:?Specify the isolated output directory}
base=${2:?Specify the EXP-2106 build directory}
saved=${FRANKENMOE_CHECKPOINTS:?Specify the checkpoint input directory}
exp="$root/experiments"
include="$exp/EXP-2026-11-25-2002-bonsai-session/source"
apex="$saved/checkpoint-apex-exp1880-before-inkling"
inkling="$saved/checkpoint-inkling-exp1969-before-bonsai"
device="$saved/inkling-exp-baseline/device-attention"
mkdir -p "$out"
printf 'd42ddb4f0b80594de6efac3a0b5926edcdaf1f62fa76bf5b6925955ddc534639  %s\n' \
    "$base/frankenmoe-infer" | sha256sum --strict -c -
python3 "$src/prepare-host.py" "$base" "$out"
python3 "$root/tools/validate-cold-32k.py" instrument-bonsai "$out/backend.c" "$out/backend-cold.c"
gcc-13 -O2 -Wall -Wextra -Werror -ffunction-sections -fdata-sections \
    -I"$base" -I"$exp/EXP-2026-11-26-2106-bonsai-http-mtp/source" \
    -I"$exp/EXP-2026-11-26-2105-bonsai-ffn-graph/source" \
    -I"$saved/bonsai-mmq-k64-2000-default" -I"$include" -I"${CUDA_HOME:-/usr/local/cuda}/include" \
    -Dmain=bonsai_diagnostic_main -c "$out/backend-cold.c" -o "$out/session-unlinked.o"
objcopy --redefine-syms="$saved/bonsai-http-2003-link/rename.txt" \
    "$out/session-unlinked.o" "$out/bonsai-session.o"
gcc-13 -O2 -std=gnu11 -D_GNU_SOURCE -DI2_NUMERIC_MODULE -Werror=implicit-function-declaration \
    -ffunction-sections -fdata-sections -I"$apex/build-inputs" -I"$apex/checkpoint" \
    -I"$root/vendor/flint/ggml/include" -I"$root/vendor/flint/ggml/src" \
    -I"$root/integration/goose-standalone" -I"$root/integration/m1/host" \
    -I"$saved/inkling-apex-gate/single-link-1953" -I"$device/inkling-attention-device-input" \
    -I"$include" -Wl,--gc-sections "$inkling/source/infer-main.c" \
    "$device/inkling-moe-tile32/numeric.o" "$out/bonsai-session.o" \
    "$saved/bonsai-http-2003-link/bonsai-goose.o" "$base/mtp-goose.o" "$base/libfranken-inference.a" \
    -lm -ldl -pthread -o "$out/frankenmoe-infer"
cp "$base/pipeline.cubin" "$base/mtp.cubin" "$out/"
