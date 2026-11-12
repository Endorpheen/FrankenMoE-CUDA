#!/bin/bash
set -e
F=/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108
E=$F/experiments/EXP-2026-11-13-451-p8-02-required-fields
OUT=$F/runtime/exp451/build
mkdir -p "$OUT"
python3 "$E/audit.py"
python3 "$E/emit_sources.py"
gcc-13 -O2 -std=c11 -Wall -Wno-unused-function -Wno-misleading-indentation -Wno-unused-but-set-variable -Wno-unused-variable \
  -I"$E/source" -I"$E/tests" "$E/tests/required_fixture.c" "$E/source/required-unit.c" -o "$OUT/required-fixture"
"$OUT/required-fixture"
