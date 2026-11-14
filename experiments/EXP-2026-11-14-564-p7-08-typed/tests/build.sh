#!/bin/bash
# Gate build for EXP565 (typed-direction slice).
# Usage: build.sh [mutate|final]; mutate is -O0 and always regenerates.
# gen.o is cached on runtime.goose mtime and the optimization tag, so a fixture
# edit costs seconds instead of a full generated-C rebuild.
set -e
MODE=${1:-final}
S=/home/end0/AI/FrankenMoE-CUDA/experiments/EXP-2026-11-14-564-p7-08-typed
L=/home/end0/AI/LlamaServer/llama.cpp-flint-cuda
D=$L/build-cuda/common/CMakeFiles/llama-common.dir
OUT=$S/build
G=/home/end0/AI/LlamaServer/toolchain/goose-2026.3-string/goose
if [ "$MODE" = "mutate" ]; then OPT=-O0; BIN=$OUT/exp565-mutate; else OPT=-O1; BIN=$OUT/exp565-gate; fi
mkdir -p "$OUT"
ms() { echo $(( ($1 - $2) / 1000000 )); }
T0=$(date +%s%N)
if [ "$MODE" = "mutate" ] || [ ! -f "$OUT/gen.o" ] || [ "$S/source/runtime.goose" -nt "$OUT/gen.o" ] || [ "$(cat "$OUT/.opt" 2>/dev/null)" != "$OPT" ]; then
  timeout 300 "$G" --standalone -o "$S/source/runtime-gen.c" --header "$S/source/runtime-gen.h" "$S/source/runtime.goose" >/dev/null 2>&1
  timeout 300 gcc-13 $OPT -std=c11 -c "$S/source/runtime-gen.c" -o "$OUT/gen.o"
  echo "$OPT" > "$OUT/.opt"
fi
T1=$(date +%s%N)
FIXTURE_O=$OUT/fixture.o
if [ "$MODE" = "mutate" ] || [ ! -f "$FIXTURE_O" ] || [ "$S/tests/fixture.cpp" -nt "$FIXTURE_O" ]; then
  timeout 300 g++-13 -O2 -std=c++17 -I"$S/source" -I"$L/common" -I"$L/include" -I"$L/src" -I"$L/ggml/include" -c "$S/tests/fixture.cpp" -o "$FIXTURE_O"
  FS=rebuilt
else
  FS=cached
fi
T2=$(date +%s%N)
timeout 300 g++-13 "$FIXTURE_O" "$OUT/gen.o" \
  "$D/jinja/lexer.cpp.o" "$D/jinja/parser.cpp.o" "$D/jinja/runtime.cpp.o" \
  "$D/jinja/value.cpp.o" "$D/jinja/string.cpp.o" "$D/jinja/caps.cpp.o" \
  "$D/unicode.cpp.o" "$D/json.cpp.o" "$D/trie.cpp.o" \
  "$L/build-cuda/bin/libggml-base.so.0.27.0" -L"$L/build-cuda/bin" -lllama-common \
  -Wl,-rpath,"$L/build-cuda/bin" -o "$BIN"
T3=$(date +%s%N)
set +e
timeout 300 "$BIN"
RC=$?
set -e
T4=$(date +%s%N)
echo "build mode=$MODE opt=$OPT fixture=$FS gen_ms=$(ms $T1 $T0) fixture_ms=$(ms $T2 $T1) link_ms=$(ms $T3 $T2) run_ms=$(ms $T4 $T3) total_ms=$(ms $T4 $T0)"
exit $RC
