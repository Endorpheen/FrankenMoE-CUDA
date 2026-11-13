#!/bin/bash
set -e
F=/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108
E=$F/experiments/EXP-2026-11-13-476-p7-09-bounds
L=/home/end0/AI/LlamaServer/llama.cpp-flint-cuda
D=$L/build-cuda/common/CMakeFiles/llama-common.dir
B=$L/build-cuda/bin
OUT=$F/runtime/exp476/build
G=/home/end0/AI/LlamaServer/toolchain/goose-2026.3-string/goose
mkdir -p "$OUT"
"$G" --standalone -o "$E/source/bounds-gen.c" --header "$E/source/bounds-gen.h" "$E/source/bounds.goose"
gcc-13 -O2 -std=c11 -c "$E/source/bounds-gen.c" -o "$OUT/gen.o"
g++-13 -O2 -std=c++17 -I"$E/source" -I"$L/common" -I"$L/ggml/include" \
    -c "$E/tests/bounds_fixture.cpp" -o "$OUT/fixture.o"
g++-13 "$OUT/fixture.o" "$OUT/gen.o" \
    "$D/json-schema-to-grammar.cpp.o" "$D/json-schema.cpp.o" "$D/json.cpp.o" \
    "$D/trie.cpp.o" "$D/unicode.cpp.o" \
    "$B/libggml-base.so.0.27.0" -Wl,-rpath,"$B" -o "$OUT/exp476-fixture"
"$OUT/exp476-fixture"
