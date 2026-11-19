#!/bin/sh
# Verify the tiled kernel with one compiler: packed idiom, gate on 1 and 8
# threads, then the two benchmark profiles.
set -e
EXP=/home/end0/AI/FrankenMoE-CUDA/experiments/EXP-2026-11-18-1008-p1-12-gemv
COMPILER="$1"
LABEL="$2"
D="$EXP/build/p112-mmid-$LABEL"
GG=/home/end0/AI/LlamaServer/llama.cpp-flint-cuda/build-cuda/bin
SRC=/home/end0/AI/LlamaServer/llama.cpp-flint-cuda/ggml
mkdir -p "$D"
cd "$EXP"

echo "== packed idiom =="
TMPDIR="$EXP/tmp" GOOSE_HOME="$EXP/goose" "$COMPILER" --packed src/mmid_smoke.goose 2>/dev/null | grep -i "mmid_tiled_q4_k" || true

echo "== emit + object =="
TMPDIR="$EXP/tmp" GOOSE_HOME="$EXP/goose" "$COMPILER" -o "$D/mmid_smoke.c" src/mmid_smoke.goose
cp src/wrapper_mmid.c "$D/wrapper_mmid.c"
TMPDIR="$EXP/tmp" gcc-13 -O3 -mavx2 -mfma -fPIC -shared -w -I src -I "$D" "$D/wrapper_mmid.c" -o "$D/libmoe_mmid_tiled.so"
sha256sum "$D/libmoe_mmid_tiled.so"

echo "== gate =="
TMPDIR="$EXP/tmp" g++ -std=gnu++20 -O2 -mavx2 -mfma -I "$SRC/include" -I "$SRC/src" -w \
    src/gate_mmid.cpp -L "$GG" -lggml-cpu -lggml-base -Wl,-rpath,"$GG" \
    -L "$D" -lmoe_mmid_tiled -Wl,-rpath,"$D" -o "$D/gate_mmid"
"$D/gate_mmid" 1 2 8 2>/dev/null | grep -E "^nth="
"$D/gate_mmid" 8 2 8 2>/dev/null | grep -E "^nth="

echo "== real Apex case (cne1=1) =="
TMPDIR="$EXP/tmp" g++ -std=gnu++20 -O2 -w src/case_mmid.cpp -L "$D" -lmoe_mmid_tiled \
    -Wl,-rpath,"$D" -o "$D/case_mmid"
"$D/case_mmid" evidence-mmid/failing_case.bin

echo "== all eight groups (independent bit-exact dot) =="
TMPDIR="$EXP/tmp" g++ -std=gnu++20 -O2 -mavx2 -mfma -w src/repro_group0_fixed.cpp \
    "$D/libmoe_mmid_tiled.so" -Wl,-rpath,"$D" -o "$D/repro_fixed"
for dump in evidence-mmid/failing_case.bin evidence-mmid/failing_case_group6.bin; do
    echo "-- $dump"
    "$D/repro_fixed" "$dump" | tail -2
done

echo "== bench =="
TMPDIR="$EXP/tmp" g++ -std=gnu++20 -O2 -mavx2 -mfma -I "$SRC/include" -I "$SRC/src" -w \
    src/bench_mmid.cpp -L "$GG" -lggml-cpu -lggml-base -Wl,-rpath,"$GG" \
    -L "$D" -lmoe_mmid_tiled -Wl,-rpath,"$D" -o "$D/bench_mmid"
"$D/bench_mmid"
