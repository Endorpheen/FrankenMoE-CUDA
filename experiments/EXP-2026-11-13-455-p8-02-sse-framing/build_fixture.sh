#!/bin/bash
set -e
F=/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108
E=$F/experiments/EXP-2026-11-13-455-p8-02-sse-framing
OUT=$F/runtime/exp455/build
mkdir -p "$OUT"
python3 "$E/emit_sources.py"
gcc-13 -O2 -std=c11 -Wall -Wno-unused-function -Wno-misleading-indentation -Wno-unused-but-set-variable -Wno-unused-variable \
  -I"$E/source" -c "$E/source/sse-unit.c" -o "$OUT/sse-unit.o"
g++-13 -O2 -std=c++17 -Wall -Wextra -I"$E/source" -c "$E/tests/sse_fixture.cpp" -o "$OUT/sse-fixture.o"
g++-13 "$OUT/sse-fixture.o" "$OUT/sse-unit.o" -o "$OUT/sse-fixture"
"$OUT/sse-fixture"
