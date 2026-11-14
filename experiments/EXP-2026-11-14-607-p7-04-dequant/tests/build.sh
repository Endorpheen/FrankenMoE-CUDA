#!/bin/bash
set -euo pipefail
S=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
L=${FG_LLAMA:-/home/end0/AI/LlamaServer/llama.cpp-flint-cuda}
GOOSE=${GOOSE:-/home/end0/AI/LlamaServer/toolchain/goose-2026.3-string/goose}
OUT=$S/build
mkdir -p "$OUT"
"$GOOSE" --standalone -o "$OUT/g.c" --header "$OUT/g.h" "$S/source/quant-dequant.goose"
gcc-13 -O0 -std=c11 -w -c "$OUT/g.c" -o "$OUT/g.o"
g++-13 -O1 -std=c++17 -I"$OUT" -I"$L/ggml/include" "$S/tests/fixture.cpp" "$OUT/g.o" \
    -L"$L/build-cuda/bin" -lggml-base -Wl,-rpath,"$L/build-cuda/bin" -o "$OUT/gate"
"$OUT/gate"
