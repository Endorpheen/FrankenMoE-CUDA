#!/usr/bin/env bash
# Universal gate build. Works from any working directory.
#
# Layout: <experiment>/source/runtime.goose, <experiment>/tests/fixture.cpp
# Usage:  bash tests/build.sh [final|mutate]
#
# final  -O1, cached gen.o keyed by source SHA + optimisation + compiler
# mutate -O0, always regenerates (a mutation changes the source)
set -euo pipefail
MODE=${1:-final}
E="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
F="$(cd "$E/../.." && pwd)"
L=${FG_LLAMA:-/home/end0/AI/LlamaServer/llama.cpp-flint-cuda}
G=${FG_GOOSE:-/home/end0/AI/LlamaServer/toolchain/goose-2026.3-string/goose}
OUT=${FG_OUT:-$F/runtime/$(basename "$E")/build}
SRC=$E/source/runtime.goose
FIX=$E/tests/fixture.cpp
LIBS=${FG_LIBS:-}
if [ "$MODE" = "mutate" ]; then OPT=-O0; BIN=$OUT/gate-mutate; else OPT=-O1; BIN=$OUT/gate; fi
mkdir -p "$OUT"
ms() { echo $(( ($1 - $2) / 1000000 )); }

T0=$(date +%s%N)
KEY=$(sha256sum "$SRC" | cut -d' ' -f1)"-$OPT-$("$G" --version 2>/dev/null | head -1 | tr ' ' '_')"
if [ "$MODE" = "mutate" ] || [ ! -f "$OUT/gen.o" ] || [ "$(cat "$OUT/.key" 2>/dev/null)" != "$KEY" ]; then
  timeout 300 "$G" --standalone -o "$E/source/runtime-gen.c" --header "$E/source/runtime-gen.h" "$SRC" >/dev/null 2>&1
  timeout 300 gcc-13 $OPT -std=c11 -c "$E/source/runtime-gen.c" -o "$OUT/gen.o"
  printf '%s' "$KEY" > "$OUT/.key"
fi
T1=$(date +%s%N)
if [ "$MODE" = "mutate" ] || [ ! -f "$OUT/fixture.o" ] || [ "$FIX" -nt "$OUT/fixture.o" ]; then
  timeout 300 g++-13 -O2 -std=c++17 -I"$E/source" -I"$L/common" -I"$L/include" -I"$L/src" -I"$L/ggml/include" \
    -c "$FIX" -o "$OUT/fixture.o"
  FS=rebuilt
else
  FS=cached
fi
T2=$(date +%s%N)
# shellcheck disable=SC2086
timeout 300 g++-13 "$OUT/fixture.o" "$OUT/gen.o" $LIBS \
  -L"$L/build-cuda/bin" -lllama-common -Wl,-rpath,"$L/build-cuda/bin" -o "$BIN"
T3=$(date +%s%N)
set +e
timeout 300 "$BIN"
RC=$?
set -e
T4=$(date +%s%N)
echo "build mode=$MODE opt=$OPT fixture=$FS gen_ms=$(ms "$T1" "$T0") fixture_ms=$(ms "$T2" "$T1") link_ms=$(ms "$T3" "$T2") run_ms=$(ms "$T4" "$T3") total_ms=$(ms "$T4" "$T0")"
exit $RC
