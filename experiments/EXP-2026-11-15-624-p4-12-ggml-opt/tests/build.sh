#!/usr/bin/env bash
# Gate build for the ggml-opt slice (P4-12 final slice).
#
# Layout: <experiment>/source/ggml-opt.goose, <experiment>/tests/{oracle,fixture}.cpp
# Usage:  bash tests/build.sh [final|mutate]
#
# final  -O1 for the generated port, cached objects keyed by source SHA
# mutate -O0, always regenerates (a mutation changes the source)
#
# The experiment root comes from this script's location, never from a hard-coded
# /tmp path. FG_LLAMA, FG_NATIVE, FG_GOOSE and FG_OUT override the rest.
set -euo pipefail
MODE=${1:-final}
E="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
L=${FG_LLAMA:-/home/end0/AI/LlamaServer/llama.cpp-flint-cuda}
N=${FG_NATIVE:-/home/end0/AI/FrankenMoE-CUDA/upstream/llama.cpp/ggml/src/ggml-opt.cpp}
G=${FG_GOOSE:-/home/end0/AI/LlamaServer/toolchain/goose-2026.3-string/goose}
OUT=${FG_OUT:-$E/build}
SRC=$E/source/ggml-opt.goose
FIX=$E/tests/fixture.cpp
ORC=$E/tests/oracle.cpp
if [ "$MODE" = "mutate" ]; then OPT=-O0; BIN=$OUT/gate-mutate; else OPT=-O1; BIN=$OUT/gate; fi
mkdir -p "$OUT"
ms() { echo $(( ($1 - $2) / 1000000 )); }
INC=(-I"$L/ggml/include" -I"$L/ggml/src" -I"$L/include" -I"$L/src" -I"$OUT")

T0=$(date +%s%N)
KEY=$(sha256sum "$SRC" | cut -d' ' -f1)"-$OPT"
if [ "$MODE" = "mutate" ] || [ ! -f "$OUT/gen.o" ] || [ "$(cat "$OUT/.key" 2>/dev/null)" != "$KEY" ]; then
  timeout 300 "$G" --standalone -o "$OUT/ggml-opt-gen.c" --header "$OUT/ggml-opt-gen.h" "$SRC" >/dev/null 2>&1
  timeout 300 gcc-13 $OPT -std=c11 -c "$OUT/ggml-opt-gen.c" -o "$OUT/gen.o"
  printf '%s' "$KEY" > "$OUT/.key"
fi
T1=$(date +%s%N)

NKEY=$(sha256sum "$N" "$ORC" | sha256sum | cut -d' ' -f1)
if [ "$MODE" = "mutate" ] || [ ! -f "$OUT/oracle.o" ] || [ "$(cat "$OUT/.nkey" 2>/dev/null)" != "$NKEY" ]; then
  timeout 300 g++-13 -O2 -std=c++17 "${INC[@]}" -DFG_OPT_CPP="\"$N\"" -c "$ORC" -o "$OUT/oracle.o"
  printf '%s' "$NKEY" > "$OUT/.nkey"
fi
T2=$(date +%s%N)

if [ "$MODE" = "mutate" ] || [ ! -f "$OUT/fixture.o" ] || [ "$FIX" -nt "$OUT/fixture.o" ]; then
  timeout 300 g++-13 -O2 -std=c++17 "${INC[@]}" -c "$FIX" -o "$OUT/fixture.o"
fi
T3=$(date +%s%N)

timeout 300 g++-13 "$OUT/fixture.o" "$OUT/oracle.o" "$OUT/gen.o" \
  -L"$L/build-cuda/bin" -lggml-base -lggml-cpu -lm -Wl,-rpath,"$L/build-cuda/bin" -o "$BIN"
T4=$(date +%s%N)
set +e
timeout 300 "$BIN"
RC=$?
set -e
T5=$(date +%s%N)
echo "build mode=$MODE opt=$OPT gen_ms=$(ms "$T1" "$T0") oracle_ms=$(ms "$T2" "$T1") fixture_ms=$(ms "$T3" "$T2") link_ms=$(ms "$T4" "$T3") run_ms=$(ms "$T5" "$T4") total_ms=$(ms "$T5" "$T0")"
exit $RC
