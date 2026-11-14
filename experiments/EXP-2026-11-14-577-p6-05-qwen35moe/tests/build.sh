#!/bin/bash
# Structure gate for the qwen35moe builder draft (P6-05 architecture 1).
#
#   tests/build.sh
#
# No native library: the draft is pure plan/shape decisions.
set -euo pipefail

S=$(cd "$(dirname "$0")/.." && pwd)
G=${GOOSE_BIN:-/home/end0/AI/LlamaServer/toolchain/goose-2026.3-string/goose}
CC=${CC:-gcc-13}
CXX=${CXX:-g++-13}
OPT=${OPT:--O1}
OUT=${OUT_DIR:-$S/build}
BIN=$OUT/fg579-arch1

mkdir -p "$OUT"

need_gen=0
if [ ! -f "$OUT/gen.o" ] || [ ! -f "$OUT/runtime-gen.h" ]; then
    need_gen=1
elif [ "$S/source/qwen35moe.goose" -nt "$OUT/gen.o" ] || [ "$S/source/base/model_graph.goose" -nt "$OUT/gen.o" ]; then
    need_gen=1
elif [ "$(cat "$OUT/.opt" 2>/dev/null || true)" != "$OPT" ]; then
    need_gen=1
fi

if [ "$need_gen" = 1 ]; then
    echo "[fg579] goose --standalone"
    "$G" --standalone -o "$OUT/runtime-gen.c" --header "$OUT/runtime-gen.h" "$S/source/qwen35moe.goose"
    echo "[fg579] gcc $OPT"
    "$CC" "$OPT" -std=c11 -c "$OUT/runtime-gen.c" -o "$OUT/gen.o"
    echo "$OPT" > "$OUT/.opt"
else
    echo "[fg579] gen.o cached"
fi

echo "[fg579] g++ fixture"
"$CXX" "$OPT" -std=c++17 -I"$OUT" -c "$S/tests/fixture.cpp" -o "$OUT/fixture.o"

echo "[fg579] link"
"$CXX" "$OUT/fixture.o" "$OUT/gen.o" -o "$BIN"

echo "[fg579] run"
"$BIN"
