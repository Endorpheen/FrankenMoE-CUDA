#!/bin/bash
set -e
F=/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108
E=$F/experiments/EXP-2026-11-13-458-p9-02-file-io
OUT=$F/runtime/exp458/build
mkdir -p "$OUT"
python3 "$E/emit_sources.py"
gcc-13 -O2 -std=c11 -Wall -Wno-unused-function -Wno-misleading-indentation -Wno-unused-but-set-variable -Wno-unused-variable \
  -I"$E/source" -c "$E/source/io-unit.c" -o "$OUT/unit.o"
g++-13 -O2 -std=c++17 -Wall -Wextra -I"$E/source" -c "$E/tests/io_fixture.cpp" -o "$OUT/fixture.o"
g++-13 "$OUT/fixture.o" "$OUT/unit.o" -o "$OUT/io-fixture"
"$OUT/io-fixture"
