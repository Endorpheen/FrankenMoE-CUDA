#!/bin/bash
set -e
F=/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108
E=$F/experiments/EXP-2026-11-13-456-p8-02-gcp-props
OUT=$F/runtime/exp456/build
mkdir -p "$OUT"
python3 "$E/emit_sources.py"
for m in gcp props; do
  gcc-13 -O2 -std=c11 -Wall -Wno-unused-function -Wno-misleading-indentation -Wno-unused-but-set-variable -Wno-unused-variable \
    -I"$E/source" -c "$E/source/$m-unit.c" -o "$OUT/$m-unit.o"
done
g++-13 -O2 -std=c++17 -Wall -Wextra -I"$E/source" -c "$E/tests/gcp_props_fixture.cpp" -o "$OUT/fixture.o"
g++-13 "$OUT/fixture.o" "$OUT/gcp-unit.o" "$OUT/props-unit.o" -o "$OUT/gcp-props-fixture"
"$OUT/gcp-props-fixture"
