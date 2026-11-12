#!/bin/bash
set -e
F=/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108
E=$F/experiments/EXP-2026-11-13-452-p8-02-multipart
OUT=$F/runtime/exp452/build
mkdir -p "$OUT"
python3 "$E/emit_sources.py"
gcc-13 -O2 -std=c11 -Wall -Wno-unused-function -Wno-misleading-indentation -Wno-unused-but-set-variable -Wno-unused-variable \
  -I"$E/source" "$E/tests/multipart_fixture.c" "$E/source/multipart-unit.c" -o "$OUT/multipart-fixture"
cd "$OUT" && ./multipart-fixture
python3 "$E/check.py"
