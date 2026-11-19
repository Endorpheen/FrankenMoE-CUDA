#!/bin/sh
# Build the EXP1008 Q4_K kernel with one compiler and run gate + benchmarks.
# Usage: run_p112.sh <compiler-binary> <label>
set -e
EXP=/home/end0/AI/FrankenMoE-CUDA/experiments/EXP-2026-11-18-1008-p1-12-gemv
COMPILER="$1"
LABEL="$2"
DIR="$EXP/build/p112-$LABEL"
mkdir -p "$DIR"
cd "$EXP"

TMPDIR="$EXP/tmp" GOOSE_HOME="$EXP/goose" "$COMPILER" -o "$DIR/gemv_smoke.c" src/gemv_smoke.goose
echo "emitted: $(grep -c '_mm256_\|_mm_' "$DIR/gemv_smoke.c") mm-ops, $(grep -c 'gg_packed_' "$DIR/gemv_smoke.c") packed refs"

TMPDIR="$EXP/tmp" gcc-13 -O3 -mavx2 -mfma -fPIC -shared -w -I src -I "$DIR" src/wrapper.c -o "$DIR/libmoe_gemv.so"
echo "library: $DIR/libmoe_gemv.so"
