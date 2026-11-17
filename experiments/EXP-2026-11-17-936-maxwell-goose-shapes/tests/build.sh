#!/usr/bin/env bash
# Differential gate for the Maxwell decision module.
# No llama.cpp dependency: the module is pure decisions and libm.
# Usage: bash tests/build.sh [final|mutate]
set -euo pipefail
MODE=${1:-final}
E="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
F="$(cd "$E/../.." && pwd)"
G=${FG_GOOSE:-/home/end0/AI/LlamaServer/toolchain/goose-2026.3-string/goose}
OUT=${FG_OUT:-$F/runtime/$(basename "$E")/build}
SRC=$E/source/runtime.goose
GEN=$E/source/runtime-gen.c
HDR=$E/source/runtime-gen.h
FIX=$E/tests/fixture.cpp
if [ "$MODE" = "mutate" ]; then OPT=-O0; BIN=$OUT/gate-mutate; else OPT=-O1; BIN=$OUT/gate; fi
mkdir -p "$OUT"

KEY=$(sha256sum "$SRC" | cut -d' ' -f1)"-$OPT-$("$G" version 2>/dev/null | head -1 | tr ' ' '_')"
if [ "$MODE" = "mutate" ] || [ ! -f "$OUT/gen.o" ] || [ "$(cat "$OUT/.key" 2>/dev/null)" != "$KEY" ]; then
  timeout 300 "$G" --standalone -o "$GEN" --header "$HDR" "$SRC" >/dev/null
  timeout 300 gcc-13 $OPT -std=c11 -c "$GEN" -o "$OUT/gen.o"
  printf '%s' "$KEY" > "$OUT/.key"
fi
if [ "$MODE" = "mutate" ] || [ ! -f "$OUT/fixture.o" ] || [ "$FIX" -nt "$OUT/fixture.o" ]; then
  timeout 300 g++-13 -O2 -std=c++17 -I"$E/source" -c "$FIX" -o "$OUT/fixture.o"
fi
timeout 300 g++-13 "$OUT/fixture.o" "$OUT/gen.o" -lm -o "$BIN"
timeout 300 "$BIN"
