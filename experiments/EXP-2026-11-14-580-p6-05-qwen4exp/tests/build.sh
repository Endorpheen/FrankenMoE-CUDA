#!/bin/bash
# Structure gate for the qwen4exp builder draft (P6-05 architecture 2).
#
#   tests/build.sh
#
# No native library: the draft is pure plan/shape decisions. The root is
# derived from this script's location, so no absolute paths are stored.
set -euo pipefail

S=$(cd "$(dirname "$0")/.." && pwd)
G=${GOOSE_BIN:-/home/end0/AI/LlamaServer/toolchain/goose-2026.3-string/goose}
CC=${CC:-gcc-13}
CXX=${CXX:-g++-13}
OPT=${OPT:--O1}
OUT=${OUT_DIR:-$S/build}
BIN=$OUT/fg579-arch2

mkdir -p "$OUT"

need_gen=0
if [ ! -f "$OUT/gen.o" ] || [ ! -f "$OUT/runtime-gen.h" ]; then
    need_gen=1
elif [ "$S/source/qwen4exp.goose" -nt "$OUT/gen.o" ] || [ "$S/source/base/model_graph.goose" -nt "$OUT/gen.o" ]; then
    need_gen=1
elif [ "$(cat "$OUT/.opt" 2>/dev/null || true)" != "$OPT" ]; then
    need_gen=1
fi

if [ "$need_gen" = 1 ]; then
    echo "[fg579a2] goose --standalone"
    "$G" --standalone -o "$OUT/runtime-gen.c" --header "$OUT/runtime-gen.h" "$S/source/qwen4exp.goose"
    echo "[fg579a2] gcc $OPT"
    "$CC" "$OPT" -std=c11 -c "$OUT/runtime-gen.c" -o "$OUT/gen.o"
    echo "$OPT" > "$OUT/.opt"
else
    echo "[fg579a2] gen.o cached"
fi

echo "[fg579a2] g++ fixture"
"$CXX" "$OPT" -std=c++17 -I"$OUT" -c "$S/tests/fixture.cpp" -o "$OUT/fixture.o"

echo "[fg579a2] link"
"$CXX" "$OUT/fixture.o" "$OUT/gen.o" -o "$BIN"

echo "[fg579a2] run"
"$BIN"
