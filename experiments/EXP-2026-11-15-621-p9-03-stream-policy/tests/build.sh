#!/usr/bin/env bash
# Differential gate for P9-03 (dense weights and expert streaming/LRU sources).
#
# The three slices are combined by source/stream_policy.goose, so one generated
# C file carries every export: the three generated headers declare the same
# slice/struct typedefs and cannot be included side by side.
#
# Usage: bash tests/build.sh [final|mutate]
# FG_GOOSE, FG_CXX, FG_CC, FG_OUT override the defaults.
set -euo pipefail
MODE=${1:-final}
E="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
G=${FG_GOOSE:-/home/end0/AI/LlamaServer/toolchain/goose-2026.3-string/goose}
CXX=${FG_CXX:-g++-13}
CC=${FG_CC:-gcc-13}
OUT=${FG_OUT:-$E/build}
SRC=$E/source/stream_policy.goose
FIX=$E/tests/fixture.cpp
if [ "$MODE" = "mutate" ]; then OPT=-O0; BIN=$OUT/gate-mutate; else OPT=-O1; BIN=$OUT/gate; fi
mkdir -p "$OUT"

# Compatibility check 1: every slice typechecks on its own.
for f in "$E"/source/expert_lru.goose "$E"/source/dense_place.goose "$E"/source/stream_plan.goose; do
  "$G" check "$f" >/dev/null
done
# ... and as the combined unit that is actually built.
"$G" check "$SRC" >/dev/null
"$G" --standalone -o "$OUT/g.c" --header "$OUT/g.h" "$SRC" >/dev/null

# Compatibility check 2: no export name is declared twice.
DUPS=$(grep -oE '\b(xl|dw|sp)_[a-z0-9_]+' "$OUT/g.h" | sort | uniq -d || true)
if [ -n "$DUPS" ]; then
  echo "duplicate exports: $DUPS" >&2
  exit 1
fi

"$CC" $OPT -std=c11 -w -c "$OUT/g.c" -o "$OUT/g.o"
"$CXX" -O1 -std=c++17 -I"$OUT" "$FIX" "$OUT/g.o" -o "$BIN"
"$BIN"
