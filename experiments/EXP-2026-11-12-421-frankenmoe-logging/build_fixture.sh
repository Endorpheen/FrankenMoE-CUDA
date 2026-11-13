#!/bin/bash
# Rebuild the FrankenMoE log formatter fixture.
set -e
F=/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108
E=$F/experiments/EXP-2026-11-12-421-frankenmoe-logging
OUT=$F/runtime/exp421/build
mkdir -p "$OUT"
/usr/bin/gcc-13 -O2 -std=c11 -I"$E/source" \
  "$E/tests/logfmt_fixture.c" "$E/source/logfmt-unit.c" -o "$OUT/logfmt_fixture"
