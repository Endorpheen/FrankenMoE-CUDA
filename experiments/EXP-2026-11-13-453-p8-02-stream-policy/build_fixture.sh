#!/bin/bash
set -e
F=/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108
E=$F/experiments/EXP-2026-11-13-453-p8-02-stream-policy
OUT=$F/runtime/exp453/build
mkdir -p "$OUT"
python3 "$E/emit_sources.py"
gcc-13 -O2 -std=c11 -c -I"$E/source" "$E/source/stream-unit.c" -o "$OUT/stream-unit.o"
g++-13 -O2 -std=c++17 -Wall -Wextra -I"$E/source" -c "$E/tests/stream_fixture.cpp" -o "$OUT/stream-fixture.o"
g++-13 "$OUT/stream-fixture.o" "$OUT/stream-unit.o" -o "$OUT/stream-fixture"
"$OUT/stream-fixture"
