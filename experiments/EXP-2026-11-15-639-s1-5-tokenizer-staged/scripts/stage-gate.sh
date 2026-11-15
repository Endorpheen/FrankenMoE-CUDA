#!/bin/bash
# Staged S1.5 gate: the Goose tokenizer decisions against the native text.
#
#   scripts/stage-gate.sh          build and run the gate for both builds
#
# The oracle sources are cut out of the tree on every run: the staged
# llama-vocab.cpp/common.cpp for the "staged" oracle and the pristine reference
# copies for the "base" one. Nothing is hand-transcribed.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
CXX=${CXX:-g++}
CXXFLAGS=${CXXFLAGS:--O1 -std=c++17 -Wall -Wextra}
OUT=${OUT:-$ROOT/build-gate}

cd "$ROOT"
mkdir -p "$OUT"

LLAMA_SRC="$ROOT/llama.cpp/src"
INCLUDES=(-I"$OUT"
          -I"$LLAMA_SRC"
          -I"$ROOT/llama.cpp/include"
          -I"$ROOT/llama.cpp/ggml/include"
          -I"$ROOT/llama.cpp/common")

# 1. Oracle sources, from the staged tree and from the pristine reference.
python3 tests/make-native.py reference/llama-vocab.cpp.orig reference/common.cpp.orig \
    reference/llama-vocab.h.orig llama.cpp/include/llama.h "$OUT" base
python3 tests/make-native.py llama.cpp/src/llama-vocab.cpp llama.cpp/common/common.cpp \
    llama.cpp/src/llama-vocab.h llama.cpp/include/llama.h "$OUT" staged

# The name sweep: every model and pre-tokenizer name of the staged tree.
python3 tests/gen-cases.py llama.cpp/src/llama-vocab.cpp "$OUT/sweep-cases.h"

# 2. Oracle objects.
for tag in base staged; do
    for part in dispatch partition strip; do
        "$CXX" $CXXFLAGS "${INCLUDES[@]}" -c "$OUT/native-$part-$tag.cpp" \
            -o "$OUT/native-$part-$tag.o"
    done
done

# 3. The gate, once per build.
build_gate() {
    local mode=$1 defs=()
    local bin="$OUT/fg-goose-tokenizer-smoke-$mode"
    if [ "$mode" = on ]; then
        defs=(-DFG_GOOSE_TOKENIZER=1)
    fi
    "$CXX" $CXXFLAGS "${INCLUDES[@]}" "${defs[@]}" \
        tests/fg_goose_tokenizer_smoke.cpp \
        "$OUT"/native-*-base.o "$OUT"/native-*-staged.o \
        -L"$ROOT/build-$mode/bin" -lllama-common -lllama -lggml -lggml-base -lggml-cpu \
        -Wl,-rpath,"$ROOT/build-$mode/bin" \
        -o "$bin"
    echo "built $bin"
}

build_gate off
build_gate on

echo "--- OFF (native bodies; staged extraction vs pristine extraction)"
"$OUT/fg-goose-tokenizer-smoke-off"

echo "--- ON (Goose decisions; bridge vs staged extraction)"
"$OUT/fg-goose-tokenizer-smoke-on"

# Every case record, for the evidence trail.
"$OUT/fg-goose-tokenizer-smoke-off" --dump > "$OUT/cases-off.txt"
"$OUT/fg-goose-tokenizer-smoke-on"  --dump > "$OUT/cases-on.txt"
echo "wrote $OUT/cases-off.txt and $OUT/cases-on.txt"
