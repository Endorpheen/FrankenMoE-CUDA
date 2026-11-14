#!/bin/bash
# Structure gate for the kimi-linear builder draft (P6-05 architecture 11).
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
BIN=$OUT/fg-arch11

mkdir -p "$OUT"

need_gen=0
if [ ! -f "$OUT/gen.o" ] || [ ! -f "$OUT/runtime-gen.h" ]; then
    need_gen=1
elif [ "$S/source/kimi-linear.goose" -nt "$OUT/gen.o" ] || [ "$S/source/base/model_graph.goose" -nt "$OUT/gen.o" ]; then
    need_gen=1
elif [ "$(cat "$OUT/.opt" 2>/dev/null || true)" != "$OPT" ]; then
    need_gen=1
fi

if [ "$need_gen" = 1 ]; then
    echo "[fg-arch11] goose --standalone"
    "$G" --standalone -o "$OUT/runtime-gen.c" --header "$OUT/runtime-gen.h" "$S/source/kimi-linear.goose"
    echo "[fg-arch11] gcc $OPT"
    "$CC" "$OPT" -std=c11 -c "$OUT/runtime-gen.c" -o "$OUT/gen.o"
    echo "$OPT" > "$OUT/.opt"
else
    echo "[fg-arch11] gen.o cached"
fi

echo "[fg-arch11] g++ fixture"
"$CXX" "$OPT" -std=c++17 -I"$OUT" -c "$S/tests/fixture.cpp" -o "$OUT/fixture.o"

echo "[fg-arch11] link"
"$CXX" "$OUT/fixture.o" "$OUT/gen.o" -o "$BIN"

echo "[fg-arch11] run"
"$BIN"
