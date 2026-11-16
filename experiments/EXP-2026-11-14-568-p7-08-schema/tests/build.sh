#!/bin/bash
# Build and run the FG573 schema fixture.
#
#   goose --standalone -> runtime-gen.c/.h -> gcc -O0 -> g++ fixture -> link
#
# The generated C/object are cached in build/ and rebuilt only when source/format-grammar.goose
# changes. No generated file is written next to the sources.
set -euo pipefail

SUB=$(cd "$(dirname "$0")/.." && pwd)
REPO=${FG_REPO:-/home/end0/AI/FrankenMoE-CUDA}
GOOSE=${GOOSE:-/home/end0/AI/LlamaServer/toolchain/goose-2026.3-string/goose}
L=${FG_LLAMA:-/home/end0/AI/LlamaServer/llama.cpp-flint-cuda}
BIN=${FG_BIN:-/home/end0/AI/LlamaServer/llama.cpp-flint-cuda/build-cuda/bin}
OUT=$SUB/build
CC=${CC:-gcc-13}
CXX=${CXX:-g++-13}

mkdir -p "$OUT"
SHA=$(sha256sum "$SUB/source/format-grammar.goose" | awk '{print $1}')

if [ ! -f "$OUT/runtime-gen.o" ] || [ ! -f "$OUT/runtime-gen.h" ] || \
   [ "$(cat "$OUT/.gen.sha" 2>/dev/null || true)" != "$SHA" ]; then
    "$GOOSE" --standalone -o "$OUT/runtime-gen.c" --header "$OUT/runtime-gen.h" "$SUB/source/format-grammar.goose"
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
