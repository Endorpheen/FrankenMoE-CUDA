#!/usr/bin/env bash
# Gate build for EXP P4-12 (meta backend, backend policy, threading).
#
# Layout: <experiment>/source/runtime.goose, <experiment>/tests/fixture.cpp
# Usage:  bash tests/build.sh [final|mutate]
#
# final  -O1 for the generated port, cached objects keyed by source SHA
# mutate -O0, always regenerates (a mutation changes the source)
#
# The experiment root comes from this script's location, never from an
# absolute path. FG_LLAMA, FG_CORE, FG_GOOSE, FG_OUT and FG_LIBS override.
set -euo pipefail
MODE=${1:-final}
E="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
L=${FG_LLAMA:-/home/end0/AI/LlamaServer/llama.cpp-flint-cuda}
C=${FG_CORE:-/home/end0/AI/FrankenMoE-CUDA/runtime/core}
G=${FG_GOOSE:-/home/end0/AI/LlamaServer/toolchain/goose-2026.3-string/goose}
OUT=${FG_OUT:-$E/build}
SRC=$E/source/runtime.goose
FIX=$E/tests/fixture.cpp
LIBS=${FG_LIBS:-}
if [ "$MODE" = "mutate" ]; then OPT=-O0; BIN=$OUT/gate-mutate; else OPT=-O1; BIN=$OUT/gate; fi
mkdir -p "$OUT"
ms() { echo $(( ($1 - $2) / 1000000 )); }

T0=$(date +%s%N)
KEY=$(sha256sum "$SRC" | cut -d' ' -f1)"-$OPT"
if [ "$MODE" = "mutate" ] || [ ! -f "$OUT/gen.o" ] || [ "$(cat "$OUT/.key" 2>/dev/null)" != "$KEY" ]; then
  timeout 300 "$G" --standalone -o "$OUT/runtime-gen.c" --header "$OUT/runtime-gen.h" "$SRC" >/dev/null 2>&1
  timeout 300 gcc-13 $OPT -std=c11 -c "$OUT/runtime-gen.c" -o "$OUT/gen.o"
  printf '%s' "$KEY" > "$OUT/.key"
fi
T1=$(date +%s%N)

CORE_KEY=$(sha256sum "$C/src/config.cpp" "$C/include/bmoe/config.h" | sha256sum | cut -d' ' -f1)
if [ ! -f "$OUT/config.o" ] || [ "$(cat "$OUT/.corekey" 2>/dev/null)" != "$CORE_KEY" ]; then
  timeout 300 g++-13 -O2 -std=c++17 -I"$C/include" -c "$C/src/config.cpp" -o "$OUT/config.o"
  printf '%s' "$CORE_KEY" > "$OUT/.corekey"
fi
T2=$(date +%s%N)

if [ "$MODE" = "mutate" ] || [ ! -f "$OUT/fixture.o" ] || [ "$FIX" -nt "$OUT/fixture.o" ]; then
  timeout 300 g++-13 -O2 -std=c++17 -I"$OUT" -I"$L/ggml/include" -I"$L/ggml/src" -I"$L/include" -I"$L/src" \
    -I"$C/include" -c "$FIX" -o "$OUT/fixture.o"
  FS=rebuilt
else
  FS=cached
fi
T3=$(date +%s%N)

# shellcheck disable=SC2086
timeout 300 g++-13 "$OUT/fixture.o" "$OUT/gen.o" "$OUT/config.o" $LIBS \
  -L"$L/build-cuda/bin" -lggml -lggml-base -Wl,-rpath,"$L/build-cuda/bin" -o "$BIN"
T4=$(date +%s%N)
set +e
timeout 300 "$BIN"
RC=$?
set -e
T5=$(date +%s%N)
echo "build mode=$MODE opt=$OPT fixture=$FS gen_ms=$(ms "$T1" "$T0") core_ms=$(ms "$T2" "$T1") fixture_ms=$(ms "$T3" "$T2") link_ms=$(ms "$T4" "$T3") run_ms=$(ms "$T5" "$T4") total_ms=$(ms "$T5" "$T0")"
exit $RC
