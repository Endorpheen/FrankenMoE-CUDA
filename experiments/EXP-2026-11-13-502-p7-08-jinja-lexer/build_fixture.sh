#!/bin/bash
set -e
F=/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108
E=$F/experiments/EXP-2026-11-13-502-p7-08-jinja-lexer
L=/home/end0/AI/LlamaServer/llama.cpp-flint-cuda
D=$L/build-cuda/common/CMakeFiles/llama-common.dir
OUT=$F/runtime/exp502/build
G=/home/end0/AI/LlamaServer/toolchain/goose-2026.3-string/goose
mkdir -p "$OUT"
"$G" --standalone -o "$E/source/lexer-gen.c" --header "$E/source/lexer-gen.h" "$E/source/lexer.goose"
gcc-13 -O2 -std=c11 -c "$E/source/lexer-gen.c" -o "$OUT/gen.o"
g++-13 -O2 -std=c++17 -I"$E/source" -I"$L/common" -I"$L/include" -I"$L/src" -I"$L/ggml/include" -c "$E/tests/lexer_fixture.cpp" -o "$OUT/fixture.o"
g++-13 "$OUT/fixture.o" "$OUT/gen.o" "$D/jinja/lexer.cpp.o" "$D/unicode.cpp.o" "$L/build-cuda/bin/libggml-base.so.0.27.0" -Wl,-rpath,"$L/build-cuda/bin" -o "$OUT/exp502-fixture"
"$OUT/exp502-fixture"
