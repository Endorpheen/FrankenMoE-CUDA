#!/bin/bash
set -e
F=/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108
E=$F/experiments/EXP-2026-11-13-445-p8-01-startup-policy
OUT=$F/runtime/exp445/build
mkdir -p "$OUT"
python3 "$E/emit_sources.py"
gcc-13 -O2 -std=c11 -Wall -Wno-unused-function -Wno-misleading-indentation -Wno-unused-but-set-variable -Wno-unused-variable -I"$E/source" "$E/tests/startup_fixture.c" "$E/source/startup-unit.c" -o "$OUT/startup-fixture"
"$OUT/startup-fixture"
