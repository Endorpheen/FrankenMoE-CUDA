#!/bin/bash
# Usage: build_fixture.sh [mutate|final]
#   mutate: gcc -O0 for the generated C, reuses fixture.o (the mutation only
#           changes source/builtins.goose)
#   final : gcc -O2, the gate that must pass before the experiment closes
# Correctness does not depend on the mode: both run the same corpus.
set -e
MODE=${1:-final}
F=/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108
E=$F/experiments/EXP-2026-11-13-506-p7-08-builtins-scalar
L=/home/end0/AI/LlamaServer/llama.cpp-flint-cuda
D=$L/build-cuda/common/CMakeFiles/llama-common.dir
OUT=$F/runtime/exp506/build
G=/home/end0/AI/LlamaServer/toolchain/goose-2026.3-string/goose

if [ "$MODE" = "mutate" ]; then
  OPT=-O0
  BIN=$OUT/exp506-mutate
else
  OPT=-O2
  BIN=$OUT/exp506-fixture
fi
mkdir -p "$OUT"

ms() { echo $(( ($1 - $2) / 1000000 )); }
T0=$(date +%s%N)

"$G" --standalone -o "$E/source/builtins-gen.c" --header "$E/source/builtins-gen.h" "$E/source/builtins.goose" >/dev/null 2>&1
T1=$(date +%s%N)

gcc-13 $OPT -std=c11 -c "$E/source/builtins-gen.c" -o "$OUT/gen.o"
T2=$(date +%s%N)

FIXTURE_O=$OUT/fixture.o
if [ ! -f "$FIXTURE_O" ] || [ "$E/tests/builtins_fixture.cpp" -nt "$FIXTURE_O" ]; then
  g++-13 -O2 -std=c++17 -I"$E/source" -I"$L/common" -I"$L/include" -I"$L/src" -I"$L/ggml/include" -c "$E/tests/builtins_fixture.cpp" -o "$FIXTURE_O"
  FIXTURE_STATE=rebuilt
else
  FIXTURE_STATE=cached
fi
T3=$(date +%s%N)

g++-13 "$FIXTURE_O" "$OUT/gen.o" \
  "$D/jinja/lexer.cpp.o" "$D/jinja/parser.cpp.o" "$D/jinja/runtime.cpp.o" \
  "$D/jinja/value.cpp.o" "$D/jinja/string.cpp.o" "$D/jinja/caps.cpp.o" \
  "$D/unicode.cpp.o" "$D/json.cpp.o" "$D/trie.cpp.o" \
  "$L/build-cuda/bin/libggml-base.so.0.27.0" -Wl,-rpath,"$L/build-cuda/bin" \
  -o "$BIN"
T4=$(date +%s%N)

set +e
"$BIN"
RC=$?
set -e
T5=$(date +%s%N)

echo "build mode=$MODE opt=$OPT fixture=$FIXTURE_STATE goose_ms=$(ms $T1 $T0) gcc_ms=$(ms $T2 $T1) fixture_ms=$(ms $T3 $T2) link_ms=$(ms $T4 $T3) run_ms=$(ms $T5 $T4) total_ms=$(ms $T5 $T0)"
exit $RC
