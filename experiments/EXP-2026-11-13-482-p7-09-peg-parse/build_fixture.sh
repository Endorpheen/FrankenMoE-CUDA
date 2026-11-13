#!/bin/bash
set -e
F=/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108
E=$F/experiments/EXP-2026-11-13-482-p7-09-peg-parse
L=/home/end0/AI/LlamaServer/llama.cpp-flint-cuda
D=$L/build-cuda/common/CMakeFiles/llama-common.dir
OUT=$F/runtime/exp482/build
G=/home/end0/AI/LlamaServer/toolchain/goose-2026.3-string/goose
mkdir -p "$OUT"
"$G" --standalone -o "$E/source/pegparse-gen.c" --header "$E/source/pegparse-gen.h" "$E/source/pegparse.goose"
gcc-13 -O2 -std=c11 -c "$E/source/pegparse-gen.c" -o "$OUT/gen.o"
g++-13 -O2 -std=c++17 -I"$E/source" -I"$L/common" -I"$L/include" -I"$L/src" -I"$L/ggml/include" \
    -c "$E/tests/peg_parse_fixture.cpp" -o "$OUT/fixture.o"
g++-13 "$OUT/fixture.o" "$OUT/gen.o" \
    "$D/peg-parser.cpp.o" "$D/json-schema-to-grammar.cpp.o" "$D/json-schema.cpp.o" \
    "$D/json.cpp.o" "$D/trie.cpp.o" "$D/unicode.cpp.o" \
    "$L/build-cuda/bin/libllama.so.0.7.0" "$L/build-cuda/bin/libggml-base.so.0.27.0" \
    -Wl,-rpath,"$L/build-cuda/bin" -o "$OUT/exp482-fixture"
"$OUT/exp482-fixture"
