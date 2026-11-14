#!/bin/bash
# Rebuild every gate against the current copy of the canon.
#   ./build3.sh [o0]
set -e
MODE=${1:-final}
B=/home/end0/AI/FrankenMoE-CUDA/experiments/EXP-2026-11-14-567-p7-08-streaming
L=/home/end0/AI/LlamaServer/llama.cpp-flint-cuda
D=$L/build-cuda/common/CMakeFiles/llama-common.dir
G=/home/end0/AI/LlamaServer/toolchain/goose-2026.3-string/goose
if [ "$MODE" = "o0" ]; then OPT=-O0; else OPT=-O1; fi
LIBS="$D/jinja/lexer.cpp.o $D/jinja/parser.cpp.o $D/jinja/runtime.cpp.o \
 $D/jinja/value.cpp.o $D/jinja/string.cpp.o $D/jinja/caps.cpp.o \
 $D/unicode.cpp.o $D/json.cpp.o $D/trie.cpp.o \
 $L/build-cuda/bin/libggml-base.so.0.27.0 -L$L/build-cuda/bin -lllama-common \
 -Wl,-rpath,$L/build-cuda/bin"
mkdir -p "$B/run"
"$G" check "$B/source/runtime.goose" >/dev/null
"$G" --standalone -o "$B/run/gen.c" --header "$B/run/gen.h" "$B/source/runtime.goose" >/dev/null
gcc-13 $OPT -std=c11 -c "$B/run/gen.c" -o "$B/run/gen.o"
gcc-13 -O1 -std=c11 -I "$B/run" -I "$B/examples" -c "$B/examples/stream_consumer.c" -o "$B/run/stream_consumer.o"

for t in fixture diff_fixture mono_fixture contract_fixture caller_fixture; do
    g++-13 -O2 -std=c++17 -I "$B/run" -I "$B/examples" -I "$L/common" -I "$L/include" -I "$L/src" -I "$L/ggml/include" \
        -c "$B/tests/$t.cpp" -o "$B/run/$t.o"
    g++-13 "$B/run/$t.o" "$B/run/stream_consumer.o" "$B/run/gen.o" $LIBS -o "$B/run/$t"
    "$B/run/$t"
done
