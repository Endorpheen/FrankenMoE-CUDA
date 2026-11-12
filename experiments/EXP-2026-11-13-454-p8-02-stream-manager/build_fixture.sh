#!/bin/bash
set -e
F=/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108
E=$F/experiments/EXP-2026-11-13-454-p8-02-stream-manager
OUT=$F/runtime/exp454/build
mkdir -p "$OUT"
python3 "$E/emit_sources.py"
gcc-13 -O2 -std=c11 -Wall -Wno-unused-function -Wno-misleading-indentation -Wno-unused-but-set-variable -Wno-unused-variable \
  -I"$E/source" -c "$E/source/manager-unit.c" -o "$OUT/manager-unit.o"
g++-13 -O2 -std=c++17 -Wall -Wextra -I"$E/source" -c "$E/tests/manager_fixture.cpp" -o "$OUT/manager-fixture.o"
g++-13 "$OUT/manager-fixture.o" "$OUT/manager-unit.o" -o "$OUT/manager-fixture"
"$OUT/manager-fixture"
