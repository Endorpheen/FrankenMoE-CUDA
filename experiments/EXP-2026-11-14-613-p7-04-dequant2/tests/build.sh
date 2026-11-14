#!/bin/bash
# Differential gate build for P7-04 S5b: dequant closure and different-type copy.
#
#   tests/build.sh                     build source/quant-dequant2.goose and run (-O1)
#   OPT=-O0 OUT_DIR=... tests/build.sh used by tests/mutate.py and the -O0 run
#
# The port is compiled to C by goose --standalone and linked into one binary
# with the native oracle: ggml_quantize_chunk, ggml_get_type_traits,
# ggml_quantize_init and the f16/bf16 conversions from libggml-base, plus
# quantize_row_q8_0 from libggml-cpu. build/native-tables.c is regenerated from
# the native ggml-common.h by tests/make-tables.py; it is fixed test data, not
# the mutated port.
#
# No absolute temporary paths: the experiment root comes from this script.
set -euo pipefail

S=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
L=${LLAMA_DIR:-/home/end0/AI/LlamaServer/llama.cpp-flint-cuda}
G=${GOOSE_BIN:-/home/end0/AI/LlamaServer/toolchain/goose-2026.3-string/goose}
CC=${CC:-gcc-13}
CXX=${CXX:-g++-13}
SRC=${GOOSE_SRC:-$S/source/quant-dequant2.goose}
OUT=${OUT_DIR:-$S/build}
OPT=${OPT:--O1}
BIN=$OUT/fg616-dequant2

mkdir -p "$OUT"

if [ ! -f "$OUT/native-tables.c" ]; then
    python3 "$S/tests/make-tables.py" "$L/ggml/src/ggml-common.h" "$OUT/native-tables.c"
fi

echo "[fg616] goose $OPT --standalone $(basename "$SRC")"
"$G" --standalone -o "$OUT/runtime-gen.c" --header "$OUT/runtime-gen.h" "$SRC" >/dev/null

echo "[fg616] $CC $OPT runtime"
"$CC" "$OPT" -std=c11 -w -c "$OUT/runtime-gen.c" -o "$OUT/gen.o"

echo "[fg616] $CC native tables"
"$CC" -O2 -std=c11 -w -c "$OUT/native-tables.c" -o "$OUT/native-tables.o"

echo "[fg616] $CXX $OPT fixture"
"$CXX" "$OPT" -std=c++17 -w -I"$OUT" -I"$L/include" -I"$L/ggml/include" \
    -c "$S/tests/fixture.cpp" -o "$OUT/fixture.o"

echo "[fg616] link"
"$CXX" "$OUT/fixture.o" "$OUT/gen.o" "$OUT/native-tables.o" \
    -L"$L/build-cuda/bin" -lggml-cpu -lggml-base -lm \
    -Wl,-rpath,"$L/build-cuda/bin" -o "$BIN"

echo "[fg616] run"
"$BIN" ${GATE_ARG:-} 2>"$OUT/run.err"
