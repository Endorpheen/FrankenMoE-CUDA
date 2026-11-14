#!/bin/bash
# Build and run the P7-02 S4 (split and merge) gate.
#
#   source/gguf-split.goose --standalone--> build/gguf_split.c/.h
#   tests/fixture.cpp links the native libggml-base/libllama and the Goose
#   object; tests/build-native.sh provides the native gguf-split oracle.
#
# Generated C, objects, the oracle binary and the gate work directory stay in
# build/. No generated file is written next to the sources.
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
SHA=$(sha256sum "$SUB/source/gguf-split.goose" | awk '{print $1}')
KEY="$SHA-$OPT"

if [ ! -f "$OUT/gguf_split.o" ] || [ "$(cat "$OUT/.gen.key" 2>/dev/null || true)" != "$KEY" ]; then
    "$GOOSE" --standalone -o "$OUT/gguf_split.c" --header "$OUT/gguf_split.h" \
        "$SUB/source/gguf-split.goose"
    "$CC" "$OPT" -std=c11 -c "$OUT/gguf_split.c" -o "$OUT/gguf_split.o"
    printf '%s\n' "$KEY" > "$OUT/.gen.key"
fi

TOOL=$(bash "$SUB/tests/build-native.sh")

"$CXX" "$OPT" -std=c++17 -I"$OUT" -I"$L/include" -I"$L/ggml/include" \
    -c "$SUB/tests/fixture.cpp" -o "$OUT/fixture.o"
"$CXX" "$OUT/fixture.o" "$OUT/gguf_split.o" \
    -L"$BIN" -lllama -lggml-base -Wl,-rpath,"$BIN" -o "$OUT/fixture"

rm -rf "$OUT/gate-work"
# The fixture's native merge refuses an absolute gate-work path; run from the
# experiment root with a relative directory, as in the author's report.
(cd "$SUB" && "./build/fixture" "build/gate-work" "$TOOL") | tee "$OUT/gate.txt"
