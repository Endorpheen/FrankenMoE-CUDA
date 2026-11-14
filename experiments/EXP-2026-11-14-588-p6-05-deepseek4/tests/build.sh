#!/bin/bash
# Structure gate for the deepseek4 builder draft (P6-05 architecture 9).
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
BIN=$OUT/fg-arch9

mkdir -p "$OUT"

need_gen=0
if [ ! -f "$OUT/gen.o" ] || [ ! -f "$OUT/runtime-gen.h" ]; then
    need_gen=1
elif [ "$S/source/deepseek4.goose" -nt "$OUT/gen.o" ] || [ "$S/source/base/model_graph.goose" -nt "$OUT/gen.o" ]; then
    need_gen=1
elif [ "$(cat "$OUT/.opt" 2>/dev/null || true)" != "$OPT" ]; then
    need_gen=1
fi

if [ "$need_gen" = 1 ]; then
    echo "[fg-arch9] goose --standalone"
    "$G" --standalone -o "$OUT/runtime-gen.c" --header "$OUT/runtime-gen.h" "$S/source/deepseek4.goose"
    echo "[fg-arch9] gcc $OPT"
    "$CC" "$OPT" -std=c11 -c "$OUT/runtime-gen.c" -o "$OUT/gen.o"
    echo "$OPT" > "$OUT/.opt"
else
    echo "[fg-arch9] gen.o cached"
fi

echo "[fg-arch9] g++ fixture"
"$CXX" "$OPT" -std=c++17 -I"$OUT" -c "$S/tests/fixture.cpp" -o "$OUT/fixture.o"

echo "[fg-arch9] link"
"$CXX" "$OUT/fixture.o" "$OUT/gen.o" -o "$BIN"

echo "[fg-arch9] run"
"$BIN"
