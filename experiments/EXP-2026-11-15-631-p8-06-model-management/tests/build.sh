#!/usr/bin/env bash
# P8-06 differential gates. Works from any working directory.
#
# Layout: source/<slice>.goose (one module per slice), tests/gate_<slice>.cpp
# Usage:  bash tests/build.sh [final|o0|mutate] [slice...]
#
# final  -O1 for every module, objects cached by source SHA + optimisation +
#        compiler + goose version
# o0     the same gate with the module compiled at -O0, so the two levels can be
#        diffed line by line
# mutate -O0, always regenerates the modules
#
# Each slice builds its own binary: the three generated headers declare the same
# std-library typedefs, so they cannot share a translation unit.
set -euo pipefail
MODE=${1:-final}
shift || true
SLICES=${*:-model-manager hot-reload connectors}
E="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
G=${FG_GOOSE:-/home/end0/AI/LlamaServer/toolchain/goose-2026.3-string/goose}
OUT=${FG_OUT:-$E/build}
case $MODE in
  final)  OPT=-O1 ;;
  o0)     OPT=-O0 ;;
  mutate) OPT=-O0 ;;
  *) echo "unknown mode $MODE" >&2; exit 2 ;;
esac
mkdir -p "$OUT"
ms() { echo $(( ($1 - $2) / 1000000 )); }

GVER=$("$G" version 2>/dev/null | head -1 | tr ' \n' '_' || true)

T0=$(date +%s%N)
for name in $SLICES; do
  src=$E/source/$name.goose
  fix=$E/tests/gate_$name.cpp
  gen=$OUT/$name-gen
  obj=$OUT/gate-$name.o
  key=$(sha256sum "$src" | cut -d' ' -f1)"-$OPT-gcc13-$GVER"
  if [ "$MODE" = "mutate" ] || [ ! -f "$gen.o" ] || [ "$(cat "$gen.key" 2>/dev/null)" != "$key" ]; then
    timeout 300 "$G" --standalone -o "$gen.c" --header "$gen.h" "$src" >/dev/null 2>&1
    timeout 300 gcc-13 $OPT -std=c11 -w -c "$gen.c" -o "$gen.o"
    printf '%s' "$key" > "$gen.key"
  fi
  if [ "$MODE" = "mutate" ] || [ ! -f "$obj" ] || [ "$fix" -nt "$obj" ] || [ "$E/tests/common.h" -nt "$obj" ]; then
    timeout 300 g++-13 -O2 -std=c++17 -w -I"$OUT" -I"$E/tests" -c "$fix" -o "$obj"
  fi
  timeout 300 g++-13 "$obj" "$gen.o" -o "$OUT/gate-$name"
done
T1=$(date +%s%N)

set +e
RC=0
for name in $SLICES; do
  timeout 300 "$OUT/gate-$name"
  rc=$?
  if [ $rc -ne 0 ]; then RC=$rc; fi
done
set -e
T2=$(date +%s%N)
echo "build mode=$MODE opt=$OPT slices=[$SLICES] build_ms=$(ms "$T1" "$T0") run_ms=$(ms "$T2" "$T1") total_ms=$(ms "$T2" "$T0")"
exit $RC
