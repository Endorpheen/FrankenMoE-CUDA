#!/bin/bash
# Differential gate build for P7-04 S1a: type selection, fallback and imatrix policy.
#
#   tests/build.sh                 build source/quant.goose and run the gate (-O1)
#   GOOSE_SRC=... OUT_DIR=... OPT=-O0 tests/build.sh   used by tests/mutate.py
#
# The gate compiles the port to C (goose --standalone) and links it into one
# binary with the native oracle: fixture.cpp includes llama-quant.cpp, so the
# static helpers (tensor_requires_imatrix, quantize_state_impl::n_fallback) are
# callable directly, and libllama supplies the rest of the runtime.
set -euo pipefail

S=$(cd "$(dirname "$0")/.." && pwd)
L=${LLAMA_DIR:-/home/end0/AI/LlamaServer/llama.cpp-flint-cuda}
G=${GOOSE_BIN:-/home/end0/AI/LlamaServer/toolchain/goose-2026.3-string/goose}
CC=${CC:-gcc-13}
CXX=${CXX:-g++-13}
SRC=${GOOSE_SRC:-$S/source/quant.goose}
OUT=${OUT_DIR:-$S/build}
OPT=${OPT:--O1}
BIN=$OUT/fg584-quant

mkdir -p "$OUT"

echo "[fg584] goose $OPT --standalone $SRC"
"$G" "$OPT" --standalone -o "$OUT/runtime-gen.c" --header "$OUT/runtime-gen.h" "$SRC"

echo "[fg584] gcc $OPT"
"$CC" "$OPT" -std=c11 -c "$OUT/runtime-gen.c" -o "$OUT/gen.o"

echo "[fg584] g++ $OPT fixture (native llama-quant.cpp)"
"$CXX" "$OPT" -std=c++17 \
    -I"$OUT" -I"$L/src" -I"$L/include" -I"$L/ggml/include" \
    -c "$S/tests/fixture.cpp" -o "$OUT/fixture.o"

echo "[fg584] link"
"$CXX" "$OUT/fixture.o" "$OUT/gen.o" \
    -L"$L/build-cuda/bin" -lllama -lggml-base \
    -Wl,-rpath,"$L/build-cuda/bin" -o "$BIN"

echo "[fg584] run"
"$BIN" 2>"$OUT/run.err"
