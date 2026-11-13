#!/bin/bash
set -e
F=/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108
E=$F/experiments/EXP-2026-11-13-473-p7-09-refs
OUT=$F/runtime/exp473/build
G=/home/end0/AI/LlamaServer/toolchain/goose-2026.3-string/goose
mkdir -p "$OUT"
"$G" --standalone -o "$E/source/refs-gen.c" --header "$E/source/refs-gen.h" "$E/source/refs.goose"
gcc-13 -O2 -std=c11 -c "$E/source/refs-gen.c" -o "$OUT/gen.o"
g++-13 -O2 -std=c++17 -I"$E/source" -c "$E/tests/refs_fixture.cpp" -o "$OUT/fixture.o"
g++-13 "$OUT/fixture.o" "$OUT/gen.o" -o "$OUT/exp473-fixture"
"$OUT/exp473-fixture"
