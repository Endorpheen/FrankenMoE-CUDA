#!/bin/bash
set -e
F=/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108
E=$F/experiments/EXP-2026-11-13-449-p8-02-route-table
OUT=$F/runtime/exp449/build
mkdir -p "$OUT"
python3 "$E/gen_routes.py"
python3 "$E/emit_sources.py"
gcc-13 -O2 -std=c11 -Wall -Wno-unused-function -Wno-misleading-indentation -Wno-unused-but-set-variable -Wno-unused-variable \
  -I"$E/source" -I"$E/tests" "$E/tests/routes_fixture.c" "$E/source/routes-unit.c" -o "$OUT/routes-fixture"
cd "$OUT" && ./routes-fixture
python3 "$E/check.py"
