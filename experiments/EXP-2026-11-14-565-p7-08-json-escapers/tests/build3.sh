#!/bin/bash
# Gate build for the value-model escaping slice (EXP567).
# Usage: build3.sh <value|value-json> [final|mutate]
set -e
MOD=${1:?module}
MODE=${2:-final}
S="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
L=/home/end0/AI/LlamaServer/llama.cpp-flint-cuda
D=$L/build-cuda/common/CMakeFiles/llama-common.dir
OUT=/home/end0/AI/FrankenMoE-CUDA/runtime/EXP-2026-11-14-565-p7-08-json-escapers/build/$MOD
G=/home/end0/AI/LlamaServer/toolchain/goose-2026.3-string/goose
if [ "$MODE" = "mutate" ]; then OPT=-O0; BIN=$OUT/$MOD-mutate; else OPT=-O1; BIN=$OUT/$MOD-gate; fi
mkdir -p "$OUT"
ms() { echo $(( ($1 - $2) / 1000000 )); }
T0=$(date +%s%N)
if [ "$MODE" = "mutate" ] || [ ! -f "$OUT/gen.o" ] || [ "$S/source/$MOD.goose" -nt "$OUT/gen.o" ]; then
  timeout 300 "$G" --standalone -o "$OUT/gen.c" --header "$OUT/gen.h" "$S/source/$MOD.goose" >/dev/null 2>&1
  timeout 300 gcc-13 $OPT -std=c11 -c "$OUT/gen.c" -o "$OUT/gen.o"
fi
T1=$(date +%s%N)
FIXTURE_O=$OUT/fixture.o
if [ "$MODE" = "mutate" ] || [ ! -f "$FIXTURE_O" ] || [ "$S/tests/fixture.cpp" -nt "$FIXTURE_O" ]; then
  timeout 300 g++-13 -O2 -std=c++17 -I"$OUT" -I"$L/common" -I"$L/include" -I"$L/src" -I"$L/ggml/include" -c "$S/tests/fixture.cpp" -o "$FIXTURE_O"
  FS=rebuilt
else
  FS=cached
fi
T2=$(date +%s%N)
timeout 300 g++-13 "$FIXTURE_O" "$OUT/gen.o" \
  "$D/jinja/lexer.cpp.o" "$D/jinja/parser.cpp.o" "$D/jinja/runtime.cpp.o" \
  "$D/jinja/value.cpp.o" "$D/jinja/string.cpp.o" "$D/jinja/caps.cpp.o" \
  "$D/unicode.cpp.o" "$D/json.cpp.o" "$D/trie.cpp.o" \
  "$L/build-cuda/bin/libggml-base.so.0.27.0" -Wl,-rpath,"$L/build-cuda/bin" \
  -o "$BIN"
T3=$(date +%s%N)
set +e
timeout 300 "$BIN"
RC=$?
set -e
T4=$(date +%s%N)
echo "build module=$MOD mode=$MODE opt=$OPT fixture=$FS gen_ms=$(ms $T1 $T0) fixture_ms=$(ms $T2 $T1) link_ms=$(ms $T3 $T2) run_ms=$(ms $T4 $T3) total_ms=$(ms $T4 $T0)"
exit $RC
