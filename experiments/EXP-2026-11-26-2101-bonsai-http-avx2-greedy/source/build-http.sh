#!/usr/bin/env bash
set -euo pipefail

src=$(cd "$(dirname "$0")" && pwd)
root=$(git -C "$src" rev-parse --show-toplevel)
out=${1:?Specify the compiled Goose build directory}
base=${2:?Specify EXP-2096 build directory}
device_image=${3:?Specify EXP-2099 pipeline.cubin}
build=${FRANKENMOE_CHECKPOINTS:-$root/build}
cuda=${CUDA_HOME:-/usr/local/cuda}
session="$root/experiments/EXP-2026-11-25-2002-bonsai-session/source"
host="$build/bonsai-mmq-k64-2000-default"
http="$build/bonsai-http-2003-link"
apex="$build/checkpoint-apex-exp1880-before-inkling"
inkling="$build/checkpoint-inkling-exp1969-before-bonsai"
device="$build/inkling-exp-baseline/device-attention"
archive="$build/bonsai-dp4a-2012-default/libfranken-inference.a"

test -s "$out/main.o"
python3 "$root/experiments/EXP-2026-11-26-2100-bonsai-http-device-choice/source/prepare-host.py" "$base/backend.c" "$out/backend.c" \
    "$build/bonsai-dp4a-2012-default/model-host.c" "$out/model-host.c"
cat >> "$out/model-host.c" <<'C'
extern uint8_t fg_q35_topk_scan_enabled(void);

uint8_t fg_bonsai_host_scan_enabled(void) {
    if (!H.bonsai) return 0;
    const char *enabled = getenv("BONSAI_HTTP_HOST_SCAN");
    return enabled && !strcmp(enabled, "1") && fg_q35_topk_scan_enabled();
}
C
python3 "$root/tools/validate-cold-32k.py" instrument-bonsai "$out/backend.c" "$out/backend-cold.c"
gcc-13 -O2 -Wall -Wextra -Werror -ffunction-sections -fdata-sections \
    -I"$host" -I"$session" -I"$cuda/include" -Dmain=bonsai_diagnostic_main \
    -c "$out/backend-cold.c" -o "$out/session-unlinked.o"
objcopy --redefine-syms="$http/rename.txt" "$out/session-unlinked.o" "$out/bonsai-session.o"
flags=(-O2 -std=gnu11 -D_GNU_SOURCE -DI2_NUMERIC_MODULE -Werror=implicit-function-declaration
    -ffunction-sections -fdata-sections -I"$apex/build-inputs" -I"$apex/checkpoint"
    -I"$root/vendor/flint/ggml/include" -I"$root/vendor/flint/ggml/src"
    -I"$root/integration/goose-standalone" -I"$root/integration/m1/host"
    -I"$build/inkling-apex-gate/single-link-1953" -I"$device/inkling-attention-device-input" -I"$session")
gcc-13 "${flags[@]}" -c "$out/model-host.c" -o "$out/model-host.o"
cp "$archive" "$out/libfranken-inference.a"
ar r "$out/libfranken-inference.a" "$out/main.o" "$out/model-host.o"
gcc-13 "${flags[@]}" -Wl,--gc-sections "$inkling/source/infer-main.c" \
    "$device/inkling-moe-tile32/numeric.o" "$out/bonsai-session.o" \
    "$http/bonsai-goose.o" "$out/libfranken-inference.a" -lm -ldl -pthread \
    -o "$out/frankenmoe-infer"
cp "$device_image" "$out/pipeline.cubin"
