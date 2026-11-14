#!/bin/bash
# Usage: build_fixture.sh [mutate|final]; mutate uses -O0 and a cached fixture object.
set -e
MODE=${1:-final}
F=/home/end0/AI/FrankenMoE-CUDA
E=$F/experiments/EXP-2026-11-14-549-p7-08-reasoning-msgs
L=/home/end0/AI/LlamaServer/llama.cpp-flint-cuda
D=$L/build-cuda/common/CMakeFiles/llama-common.dir
OUT=$F/runtime/exp549/build
G=/home/end0/AI/LlamaServer/toolchain/goose-2026.3-string/goose
if [ "$MODE" = "mutate" ]; then OPT=-O0; BIN=$OUT/exp549-mutate; else OPT=-O2; BIN=$OUT/exp549-fixture; fi
mkdir -p "$OUT"
ms() { echo $(( ($1 - $2) / 1000000 )); }
T0=$(date +%s%N)
timeout 300 "$G" --standalone -o "$E/source/runtime-gen.c" --header "$E/source/runtime-gen.h" "$E/source/runtime.goose" >/dev/null 2>&1
T1=$(date +%s%N)
timeout 300 gcc-13 $OPT -std=c11 -c "$E/source/runtime-gen.c" -o "$OUT/gen.o"
T2=$(date +%s%N)
FIXTURE_O=$OUT/fixture.o
if [ ! -f "$FIXTURE_O" ] || [ "$E/tests/stmt_fixture.cpp" -nt "$FIXTURE_O" ]; then
  timeout 300 g++-13 -O2 -std=c++17 -DFIXTURE_DIR="\"$E/tests/templates\"" -I"$E/source" -I"$L/common" -I"$L/include" -I"$L/src" -I"$L/ggml/include" -c "$E/tests/stmt_fixture.cpp" -o "$FIXTURE_O"
  FS=rebuilt
else
  FS=cached
fi
T3=$(date +%s%N)
timeout 300 g++-13 "$FIXTURE_O" "$OUT/gen.o" \
  "$D/jinja/lexer.cpp.o" "$D/jinja/parser.cpp.o" "$D/jinja/runtime.cpp.o" \
  "$D/jinja/value.cpp.o" "$D/jinja/string.cpp.o" "$D/jinja/caps.cpp.o" \
  "$D/unicode.cpp.o" "$D/json.cpp.o" "$D/trie.cpp.o" \
  "$L/build-cuda/bin/libggml-base.so.0.27.0" -L"$L/build-cuda/bin" -lllama-common \
  -Wl,-rpath,"$L/build-cuda/bin" -o "$BIN"
T4=$(date +%s%N)
set +e
timeout 300 "$BIN"
RC=$?
set -e
T5=$(date +%s%N)
echo "build mode=$MODE opt=$OPT fixture=$FS goose_ms=$(ms $T1 $T0) gcc_ms=$(ms $T2 $T1) fixture_ms=$(ms $T3 $T2) link_ms=$(ms $T4 $T3) run_ms=$(ms $T5 $T4) total_ms=$(ms $T5 $T0)"
exit $RC
