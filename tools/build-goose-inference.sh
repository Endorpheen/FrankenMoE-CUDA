#!/bin/bash
# Link the standalone inference host and Goose CPU graph kernels with the C tensor substrate.
set -euo pipefail
ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
OUT=${1:?usage: build-goose-inference.sh output-directory [composed-goose-unit-directory]}
UNIT=${2:-$OUT/unit}
CC=${CC:-gcc-13}
AR=${AR:-ar}
mkdir -p "$OUT"
if [ "$#" -lt 2 ]; then
    bash "$ROOT/tools/build-main-runtime.sh" "$UNIT"
fi
bash "$ROOT/tools/build-goose-cpu-kernels.sh" "$OUT"
bash "$ROOT/tools/build-goose-tensor-core.sh" "$OUT"

BASE=$ROOT/vendor/flint/ggml
printf '#include "main.h"\n' > "$OUT/goose-unit.h"
FLAGS=(-O1 -std=c11 -D_GNU_SOURCE -ffunction-sections -fdata-sections -I "$OUT"
       -I "$UNIT" -I "$BASE/include" -I "$BASE/src" -I "$ROOT/integration/m1/host")
"$CC" "${FLAGS[@]}" -c "$ROOT/integration/goose-standalone/graph-execute.c" -o "$OUT/graph-execute.o"
"$CC" "${FLAGS[@]}" -c "$ROOT/integration/goose-standalone/graph-bindings.c" -o "$OUT/graph-bindings.o"
"$CC" "${FLAGS[@]}" -c "$ROOT/integration/goose-standalone/goose-host-llm.c" -o "$OUT/model-host.o"
"$CC" "${FLAGS[@]}" -c "$ROOT/integration/goose-standalone/libc-forwards.c" -o "$OUT/libc-forwards.o"
"$CC" "${FLAGS[@]}" -c "$ROOT/integration/m1/host/goose_host.c" -o "$OUT/goose-host.o"

"$AR" rcs "$OUT/libfranken-inference.a" "$UNIT/main.o" "$OUT/cpu.o" \
    "$OUT/graph-execute.o" "$OUT/graph-bindings.o" "$OUT/model-host.o" \
    "$OUT/libc-forwards.o" "$OUT/goose-host.o" "$OUT/ggml.o" "$OUT/ggml-quants.o" "$OUT/ggml-system.o"
"$CC" "${FLAGS[@]}" -I "$ROOT/integration/goose-standalone" -Wl,--gc-sections \
    "$ROOT/integration/goose-standalone/cli/infer-main.c" "$OUT/libfranken-inference.a" \
    -lm -pthread -o "$OUT/frankenmoe-infer"
printf '%s/libfranken-inference.a\n%s/frankenmoe-infer\n' "$OUT" "$OUT"
