#!/bin/bash
# Build and run the P7-02 S2 (GGUF writer) gate.
#
#   source/gguf-writer.goose --standalone--> build/gguf_writer.c/.h
#   tests/fixture.cpp links the native libggml-base and the Goose object; it
#   compares the canonical KV dump and the serialized bytes of both sides.
#
# Generated C and objects stay in build/ and are rebuilt only when the source
# changes. No generated file is written next to the sources.
set -euo pipefail

SUB=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
REPO=${FG_REPO:-/home/end0/AI/FrankenMoE-CUDA}
GOOSE=${GOOSE:-/home/end0/AI/LlamaServer/toolchain/goose-2026.3-string/goose}
L=${FG_LLAMA:-$REPO/upstream/llama.cpp}
BIN=${FG_BIN:-/home/end0/AI/LlamaServer/llama.cpp-flint-cuda/build-cuda/bin}
OUT=${FG_OUT:-$SUB/build}
CC=${CC:-gcc-13}
CXX=${CXX:-g++-13}
OPT=${OPT:--O1}

mkdir -p "$OUT"
SHA=$(sha256sum "$SUB/source/gguf-writer.goose" | awk '{print $1}')
KEY="$SHA-$OPT"

if [ ! -f "$OUT/gguf_writer.o" ] || [ "$(cat "$OUT/.gen.key" 2>/dev/null || true)" != "$KEY" ]; then
    "$GOOSE" --standalone -o "$OUT/gguf_writer.c" --header "$OUT/gguf_writer.h" \
        "$SUB/source/gguf-writer.goose"
    "$CC" "$OPT" -std=c11 -c "$OUT/gguf_writer.c" -o "$OUT/gguf_writer.o"
    printf '%s\n' "$KEY" > "$OUT/.gen.key"
fi

"$CXX" "$OPT" -std=c++17 -I"$OUT" -I"$L/ggml/include" \
    -c "$SUB/tests/fixture.cpp" -o "$OUT/fixture.o"
"$CXX" "$OUT/fixture.o" "$OUT/gguf_writer.o" \
    -L"$BIN" -lggml-base -Wl,-rpath,"$BIN" -o "$OUT/fixture" -lm

"$OUT/fixture" "$OUT/align_tmp.bin" | tee "$OUT/gate.txt"
