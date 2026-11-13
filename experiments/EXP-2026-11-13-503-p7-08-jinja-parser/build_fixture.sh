#!/bin/bash
set -e
F=/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108
E=$F/experiments/EXP-2026-11-13-503-p7-08-jinja-parser
L=/home/end0/AI/LlamaServer/llama.cpp-flint-cuda
D=$L/build-cuda/common/CMakeFiles/llama-common.dir
OUT=$F/runtime/exp503/build
G=/home/end0/AI/LlamaServer/toolchain/goose-2026.3-string/goose
mkdir -p "$OUT"
"$G" --standalone -o "$E/source/parser-gen.c" --header "$E/source/parser-gen.h" "$E/source/parser.goose"
gcc-13 -O2 -std=c11 -c "$E/source/parser-gen.c" -o "$OUT/gen.o"
g++-13 -O2 -std=c++17 -I"$E/source" -I"$L/common" -I"$L/include" -I"$L/src" -I"$L/ggml/include" -c "$E/tests/parser_fixture.cpp" -o "$OUT/fixture.o"
g++-13 "$OUT/fixture.o" "$OUT/gen.o" \
  "$D/jinja/lexer.cpp.o" "$D/jinja/parser.cpp.o" "$D/jinja/runtime.cpp.o" \
  "$D/jinja/value.cpp.o" "$D/jinja/string.cpp.o" "$D/jinja/caps.cpp.o" \
  "$D/unicode.cpp.o" "$D/json.cpp.o" "$D/trie.cpp.o" \
  "$L/build-cuda/bin/libggml-base.so.0.27.0" -Wl,-rpath,"$L/build-cuda/bin" \
  -o "$OUT/exp503-fixture"
"$OUT/exp503-fixture"
