#!/bin/bash
set -e
F=/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108
E=$F/experiments/EXP-2026-11-13-443-p7-01-gguf-parser
OUT=$F/runtime/exp443/build
mkdir -p "$OUT"
python3 "$E/emit_sources.py"
gcc-13 -O2 -std=c11 -I"$E/source" "$E/tests/gguf_fixture.c" "$E/source/gguf-unit.c" -o "$OUT/gguf-fixture"
cd "$OUT" && ./gguf-fixture
python3 "$E/check.py"
