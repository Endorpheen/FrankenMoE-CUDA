#!/bin/bash
# Differential gate build for P7-04 S3b: K, IQ, MXFP4, NVFP4 and TQ quantizers.
#
#   tests/build.sh                  build source/quant-rows2.goose and run the gate (-O1)
#   GOOSE_SRC=... OUT_DIR=... OPT=-O0 tests/build.sh    used by tests/mutate.py
#
# The port is compiled to C by goose --standalone and linked into one binary
# with the native oracle: ggml_quantize_chunk, quantize_row_q8_K_ref and
# ggml_quantize_init from libggml-base (libggml-cpu supplies the CPU init).
# build/native-tables.c is regenerated from the native ggml-quants.c by
# tests/make-tables.py and always built at -O2: it is fixed test
# infrastructure, not the mutated port, and its table construction is slow at
# -O0.
#
# No absolute temporary paths: the experiment root comes from this script.
set -euo pipefail

S=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
L=${LLAMA_DIR:-/home/end0/AI/LlamaServer/llama.cpp-flint-cuda}
G=${GOOSE_BIN:-/home/end0/AI/LlamaServer/toolchain/goose-2026.3-string/goose}
CC=${CC:-gcc-13}
CXX=${CXX:-g++-13}
SRC=${GOOSE_SRC:-$S/source/quant-rows2.goose}
OUT=${OUT_DIR:-$S/build}
OPT=${OPT:--O1}
BIN=$OUT/fg612-rows2

mkdir -p "$OUT"

if [ ! -f "$OUT/native-tables.c" ]; then
    python3 "$S/tests/make-tables.py" "$L/ggml/src/ggml-quants.c" "$OUT/native-tables.c"
fi

echo "[fg612] goose $OPT --standalone $SRC"
"$G" --standalone -o "$OUT/runtime-gen.c" --header "$OUT/runtime-gen.h" "$SRC" >/dev/null

echo "[fg612] $CC $OPT"
"$CC" "$OPT" -std=c11 -c "$OUT/runtime-gen.c" -o "$OUT/gen.o"

echo "[fg612] $CC -O2 native tables"
"$CC" -O2 -std=c11 -I"$L/include" -I"$L/ggml/include" -I"$L/ggml/src" \
    -c "$OUT/native-tables.c" -o "$OUT/native-tables.o"

echo "[fg612] $CXX $OPT fixture"
"$CXX" "$OPT" -std=c++17 -I"$OUT" -I"$L/include" -I"$L/ggml/include" \
    -c "$S/tests/fixture.cpp" -o "$OUT/fixture.o"

echo "[fg612] link"
"$CXX" "$OUT/fixture.o" "$OUT/gen.o" "$OUT/native-tables.o" \
    -L"$L/build-cuda/bin" -lggml-base -lggml-cpu -lm \
    -Wl,-rpath,"$L/build-cuda/bin" -o "$BIN"

echo "[fg612] run"
"$BIN" "${GATE_ARG:---quick}" 2>"$OUT/run.err"
