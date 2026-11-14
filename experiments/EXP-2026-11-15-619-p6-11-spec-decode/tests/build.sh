#!/usr/bin/env bash
# Differential gate for P6-11 (speculative decoding decisions), no model and no GPU: the
# fixture carries a transcription of the native functions, the Goose module carries the port,
# both evaluate the same synthetic corpus and the gate compares every field.
#
# Usage: bash tests/build.sh [final|mutate]
#   final  module at -O1, the gate the report cites
#   mutate module at -O0, always regenerated, for the mutation halves
# FG_GOOSE, FG_CC, FG_CXX, FG_OUT override the defaults; the experiment root is derived from
# this script's location, so the build works from any working directory.
set -euo pipefail
MODE=${1:-final}
E="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
G=${FG_GOOSE:-/home/end0/AI/LlamaServer/toolchain/goose-2026.3-string/goose}
CC=${FG_CC:-gcc-13}
CXX=${FG_CXX:-g++-13}
OUT=${FG_OUT:-$E/build}
SRC=$E/source/sdp.goose
FIX=$E/tests/fixture.cpp
GEN_C=$OUT/sdp-gen.c
GEN_H=$OUT/sdp-gen.h
mkdir -p "$OUT"

if [ "$MODE" = "mutate" ]; then OPT=-O0; BIN=$OUT/gate-mutate; else OPT=-O1; BIN=$OUT/gate; fi
KEY=$(sha256sum "$SRC" | cut -d' ' -f1)"-$OPT-$("$G" version 2>/dev/null | tail -1 | tr ' ' '_')"

if [ "$MODE" = "mutate" ] || [ ! -f "$OUT/gen$OPT.o" ] || [ "$(cat "$OUT/.key$OPT" 2>/dev/null)" != "$KEY" ]; then
  "$G" --standalone -o "$GEN_C" --header "$GEN_H" "$SRC"
  "$CC" "$OPT" -std=c11 -w -c "$GEN_C" -o "$OUT/gen$OPT.o"
  printf '%s' "$KEY" > "$OUT/.key$OPT"
fi

"$CXX" -O1 -std=c++17 -I"$OUT" "$FIX" "$OUT/gen$OPT.o" -o "$BIN" -lm
"$BIN"
