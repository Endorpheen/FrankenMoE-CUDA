#!/bin/bash
set -e
F=/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108
E=$F/experiments/EXP-2026-11-13-457-p9-02-gguf-offsets
OUT=$F/runtime/exp457/build
mkdir -p "$OUT"
python3 "$E/gen_expected.py"
python3 "$E/emit_sources.py"
gcc-13 -O2 -std=c11 -Wall -Wno-unused-function -Wno-misleading-indentation -Wno-unused-but-set-variable -Wno-unused-variable \
  -I"$E/source" -c "$E/source/offsets-unit.c" -o "$OUT/unit.o"
g++-13 -O2 -std=c++17 -Wall -Wextra -I"$E/source" -I"$E/tests" -c "$E/tests/offsets_fixture.cpp" -o "$OUT/fixture.o"
g++-13 "$OUT/fixture.o" "$OUT/unit.o" -o "$OUT/offsets-fixture"
"$OUT/offsets-fixture"
