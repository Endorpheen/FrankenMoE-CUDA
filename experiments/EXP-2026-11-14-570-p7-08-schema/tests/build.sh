#!/bin/bash
# Build and run the FG573 integration fixture.
#
#   source/format-grammar.goose -> runtime-gen.c/.h -> gcc -O0 ->
#   g++ fixture + libllama-common.
#
# Generated files and objects live in build/; the object is cached by the
# SHA-256 of the integration source.
set -euo pipefail

SUB="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
REPO=${FG_REPO:-/home/end0/AI/FrankenMoE-CUDA}
GOOSE=${GOOSE:-/home/end0/AI/LlamaServer/toolchain/goose-2026.3-string/goose}
L=${FG_LLAMA:-$REPO/upstream/llama.cpp}
BIN=${FG_BIN:-$REPO/build/expert-tier-franken-cuda/bin}
OUT=$SUB/build
CC=${CC:-gcc-13}
CXX=${CXX:-g++-13}
SRC=$SUB/source/format-grammar.goose

mkdir -p "$OUT"
SHA=$(sha256sum "$SRC" | awk '{print $1}')

if [ ! -f "$OUT/runtime-gen.o" ] || [ ! -f "$OUT/runtime-gen.h" ] || \
   [ "$(cat "$OUT/.gen.sha" 2>/dev/null || true)" != "$SHA" ]; then
    "$GOOSE" --standalone -o "$OUT/runtime-gen.c" --header "$OUT/runtime-gen.h" "$SRC"
    "$CC" -O0 -std=c11 -c "$OUT/runtime-gen.c" -o "$OUT/runtime-gen.o"
    printf '%s\n' "$SHA" > "$OUT/.gen.sha"
fi

"$CXX" -O0 -std=c++17 \
    -I"$OUT" -I"$L/common" -I"$L/include" -I"$L/src" -I"$L/ggml/include" \
    -c "$SUB/tests/fixture.cpp" -o "$OUT/fixture.o"

"$CXX" "$OUT/fixture.o" "$OUT/runtime-gen.o" \
    -L"$BIN" -lllama-common -lllama -lggml -lggml-base \
    -Wl,-rpath,"$BIN" -o "$OUT/fg573-schema"

exec "$OUT/fg573-schema"
