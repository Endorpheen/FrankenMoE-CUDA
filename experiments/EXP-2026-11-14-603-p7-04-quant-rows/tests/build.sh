#!/bin/bash
# Differential gate build for P7-04 S3a: the unweighted row quantizers.
#
#   tests/build.sh                  build source/quant-rows.goose and run the gate (-O1)
#   GOOSE_SRC=... OUT_DIR=... OPT=-O0 tests/build.sh    used by tests/mutate.py
#
# The port is compiled to C by goose --standalone and linked into one binary
# with the native oracle, ggml_quantize_chunk from libggml-base (libggml-cpu
# supplies ggml_quantize_init). No absolute temporary paths: the experiment
# root comes from this script's location.
set -euo pipefail

S=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
L=${LLAMA_DIR:-/home/end0/AI/LlamaServer/llama.cpp-flint-cuda}
G=${GOOSE_BIN:-/home/end0/AI/LlamaServer/toolchain/goose-2026.3-string/goose}
CC=${CC:-gcc-13}
CXX=${CXX:-g++-13}
SRC=${GOOSE_SRC:-$S/source/quant-rows.goose}
OUT=${OUT_DIR:-$S/build}
OPT=${OPT:--O1}
BIN=$OUT/fg607-rows

mkdir -p "$OUT"

echo "[fg607] goose $OPT --standalone $SRC"
"$G" --standalone -o "$OUT/runtime-gen.c" --header "$OUT/runtime-gen.h" "$SRC" >/dev/null

echo "[fg607] $CC $OPT"
"$CC" "$OPT" -std=c11 -c "$OUT/runtime-gen.c" -o "$OUT/gen.o"

echo "[fg607] $CXX $OPT fixture"
"$CXX" "$OPT" -std=c++17 -I"$OUT" -I"$L/include" -I"$L/ggml/include" \
    -c "$S/tests/fixture.cpp" -o "$OUT/fixture.o"

echo "[fg607] link"
"$CXX" "$OUT/fixture.o" "$OUT/gen.o" \
    -L"$L/build-cuda/bin" -lggml-base -lggml-cpu \
    -Wl,-rpath,"$L/build-cuda/bin" -o "$BIN"

echo "[fg607] run"
"$BIN" 2>"$OUT/run.err"
