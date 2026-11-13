#!/bin/bash
set -e
F=/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108
E=$F/experiments/EXP-2026-11-13-457-p9-02-gguf-offsets
OUT=$F/runtime/exp457/build
G=/home/end0/AI/LlamaServer/toolchain/goose-2026.3-string/goose
mkdir -p "$OUT"
"$G" -o "$E/source/offsets-gen.c" --header "$E/source/offsets-gen.h" "$E/source/offsets.goose"
if [ ! -f "$OUT/gen-runtime.c" ]; then "$G" --emit-runtime "$OUT/gen-runtime.c"; fi
gcc-13 -O2 -std=c11 -c "$OUT/gen-runtime.c" -o "$OUT/gen-runtime.o"
gcc-13 -O2 -std=c11 -I"$E/source" -c "$E/source/offsets-gen.c" -o "$OUT/gen.o"
g++-13 -O2 -std=c++17 -I"$E/source" -I"$E/tests" -c "$E/tests/offsets_fixture.cpp" -o "$OUT/fixture.o"
g++-13 "$OUT/fixture.o" "$OUT/gen.o" "$OUT/gen-runtime.o" -o "$OUT/exp457-fixture"
"$OUT/exp457-fixture"
