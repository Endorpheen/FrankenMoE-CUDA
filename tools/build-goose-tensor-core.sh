#!/bin/bash
# Build the C tensor and quantization substrate used by the Goose graph.
set -euo pipefail

ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
OUT=${1:?usage: build-goose-tensor-core.sh /home/end0/AI/LlamaServer/output-directory}
CC=${CC:-gcc-13}
AR=${AR:-ar}
BASE=$ROOT/vendor/flint/ggml
mkdir -p "$OUT"
version=$(sed -nE 's/^set\(GGML_VERSION_(MAJOR|MINOR|PATCH) ([0-9]+)\)$/\2/p' "$BASE/CMakeLists.txt" | paste -sd.)
commit=$(git -C "$ROOT" rev-parse --short HEAD)
sed -e "s/@GGML_VERSION@/$version/g" -e "s/@GGML_BUILD_COMMIT@/$commit/g" \
    "$BASE/src/ggml-version.h.in" > "$OUT/ggml-version.h"
FLAGS=(-O1 -D_GNU_SOURCE -std=c11 -ffunction-sections -fdata-sections
       -I "$OUT" -I "$BASE/include" -I "$BASE/src" -I "$BASE/src/ggml-cpu")
"$CC" "${FLAGS[@]}" -c "$BASE/src/ggml.c" -o "$OUT/ggml.o"
"$CC" "${FLAGS[@]}" -c "$BASE/src/ggml-quants.c" -o "$OUT/ggml-quants.o"
"$CC" "${FLAGS[@]}" -c "$ROOT/integration/goose-standalone/ggml-pthread.c" -o "$OUT/ggml-pthread.o"
"$AR" rcs "$OUT/libfranken-tensor.a" "$OUT/ggml.o" "$OUT/ggml-quants.o" "$OUT/ggml-pthread.o"
printf '%s/libfranken-tensor.a\n' "$OUT"
