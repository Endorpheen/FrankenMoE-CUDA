#!/bin/bash
set -e
F=/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108
E=$F/experiments/EXP-2026-11-13-480-p7-09-gbnf-parser
L=/home/end0/AI/LlamaServer/llama.cpp-flint-cuda
OUT=$F/runtime/exp480/build
G=/home/end0/AI/LlamaServer/toolchain/goose-2026.3-string/goose
mkdir -p "$OUT"
"$G" --standalone -o "$E/source/gbnf-gen.c" --header "$E/source/gbnf-gen.h" "$E/source/gbnf.goose"
gcc-13 -O2 -std=c11 -c "$E/source/gbnf-gen.c" -o "$OUT/gen.o"
g++-13 -O2 -std=c++17 -I"$E/source" -I"$L/include" -I"$L/src" -I"$L/ggml/include" \
    -c "$E/tests/gbnf_fixture.cpp" -o "$OUT/fixture.o"
g++-13 "$OUT/fixture.o" "$OUT/gen.o" \
    "$L/build-cuda/src/CMakeFiles/llama.dir/llama-grammar.cpp.o" \
    "$L/build-cuda/bin/libllama.so.0.7.0" "$L/build-cuda/bin/libggml-base.so.0.27.0" \
    -Wl,-rpath,"$L/build-cuda/bin" -o "$OUT/exp480-fixture"
"$OUT/exp480-fixture"
