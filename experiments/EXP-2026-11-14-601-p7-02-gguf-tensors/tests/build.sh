#!/bin/bash
# Build and run the P7-02 S3 (tensor descriptors + data) gate.
#
#   source/gguf-tensors.goose --standalone--> build/gguf_tensors.c/.h
#   tests/fixture.cpp links the native libggml-base and the Goose object; it
#   compares KV dumps, tensor descriptor dumps, metadata bytes and full bytes,
#   then reloads the port bytes through the native reader.
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
SHA=$(sha256sum "$SUB/source/gguf-tensors.goose" | awk '{print $1}')
KEY="$SHA-$OPT"

if [ ! -f "$OUT/gguf_tensors.o" ] || [ "$(cat "$OUT/.gen.key" 2>/dev/null || true)" != "$KEY" ]; then
    "$GOOSE" --standalone -o "$OUT/gguf_tensors.c" --header "$OUT/gguf_tensors.h" \
        "$SUB/source/gguf-tensors.goose"
    "$CC" "$OPT" -std=c11 -c "$OUT/gguf_tensors.c" -o "$OUT/gguf_tensors.o"
    printf '%s\n' "$KEY" > "$OUT/.gen.key"
fi

"$CXX" "$OPT" -std=c++17 -I"$OUT" -I"$L/ggml/include" \
    -c "$SUB/tests/fixture.cpp" -o "$OUT/fixture.o"
"$CXX" "$OUT/fixture.o" "$OUT/gguf_tensors.o" \
    -L"$BIN" -lggml-base -Wl,-rpath,"$BIN" -o "$OUT/fixture" -lm

"$OUT/fixture" "$OUT/align_tmp.bin" | tee "$OUT/gate.txt"
