#!/bin/bash
# Differential gate build for the token-aligned chat message delimiters (EXP571).
#
# Usage: tests/build.sh [--no-run]
#
# Inputs: source/runtime.goose, tests/fixture.cpp.
# Steps: goose --standalone -> runtime-gen.c/.h -> gcc -O0 -> g++
# fixture.o + libllama-common + the server token helper (server_tokens) and
# libmtmd (mtmd test chunks). gen.o and fixture.o are cached by timestamp.
#
# Environment overrides:
#   FG571_SRC    Goose source           (default <root>/source/runtime.goose)
#   FG571_OUT    build directory        (default <root>/build)
#   FG571_LLAMA  flint llama.cpp tree   (default /home/end0/AI/LlamaServer/llama.cpp-flint-cuda)
#   FG571_VOCAB  vocab-only gguf        (default $FG571_LLAMA/models/ggml-vocab-qwen35.gguf)
#   FG571_TEMPLATES  jinja templates dir (default $FG571_LLAMA/models/templates)
#   FG571_GOOSE  goose binary           (default /home/end0/AI/Fried-Goose/build/goose)
#   FG571_OPT    C/C++ optimization     (default -O0)
#   FG571_CC     C compiler             (default gcc-13)
#   FG571_CXX    C++ compiler           (default g++-13)

set -e

HERE=$(cd "$(dirname "$0")" && pwd)
ROOT=$(cd "$HERE/.." && pwd)

SRC=${FG571_SRC:-$ROOT/source/runtime.goose}
FIXTURE=$HERE/fixture.cpp
OUT=${FG571_OUT:-$ROOT/build}
L=${FG571_LLAMA:-/home/end0/AI/LlamaServer/llama.cpp-flint-cuda}
BIN_DIR=$L/build-cuda/bin
VOCAB=${FG571_VOCAB:-$L/models/ggml-vocab-qwen35.gguf}
TEMPLATES=${FG571_TEMPLATES:-$L/models/templates}
G=${FG571_GOOSE:-/home/end0/AI/Fried-Goose/build/goose}
OPT=${FG571_OPT:--O0}
CC=${FG571_CC:-gcc-13}
CXX=${FG571_CXX:-g++-13}

RUN=1
if [ "${1:-}" = "--no-run" ]; then
    RUN=0
fi

for f in "$SRC" "$FIXTURE" "$G" "$VOCAB"; do
    if [ ! -f "$f" ]; then
        echo "FG571_BUILD missing input: $f" >&2
        exit 1
    fi
done
if [ ! -d "$BIN_DIR" ]; then
    echo "FG571_BUILD missing llama build: $BIN_DIR" >&2
    exit 1
fi
if [ ! -d "$TEMPLATES" ]; then
    echo "FG571_BUILD missing templates: $TEMPLATES" >&2
    exit 1
fi

mkdir -p "$OUT"

ms() {
    echo $(( ($1 - $2) / 1000000 ))
}

T0=$(date +%s%N)

timeout 300 "$G" --standalone \
    -o "$OUT/runtime-gen.c" \
    --header "$OUT/runtime-gen.h" \
    "$SRC"
T1=$(date +%s%N)

GEN_O=$OUT/gen.o
if [ ! -f "$GEN_O" ] || [ "$OUT/runtime-gen.c" -nt "$GEN_O" ] || [ "$OUT/runtime-gen.h" -nt "$GEN_O" ]; then
    timeout 300 "$CC" $OPT -std=c11 -c "$OUT/runtime-gen.c" -o "$GEN_O"
    GS=rebuilt
else
    GS=cached
fi
T2=$(date +%s%N)

FIX_O=$OUT/fixture.o
if [ ! -f "$FIX_O" ] || [ "$FIXTURE" -nt "$FIX_O" ] || [ "$OUT/runtime-gen.h" -nt "$FIX_O" ]; then
    timeout 300 "$CXX" $OPT -std=c++17 \
        -I"$OUT" -I"$L/common" -I"$L/include" -I"$L/src" -I"$L/ggml/include" \
        -I"$L/tools/server" -I"$L/tools/mtmd" \
        -c "$FIXTURE" -o "$FIX_O"
    FS=rebuilt
else
    FS=cached
fi
T3=$(date +%s%N)

BIN=$OUT/fg571-delim
timeout 300 "$CXX" "$FIX_O" "$GEN_O" \
    -L"$BIN_DIR" -lllama-server-impl -lmtmd -lllama-common -lllama -lggml -lggml-base -lm \
    -Wl,-rpath,"$BIN_DIR" -o "$BIN"
T4=$(date +%s%N)

RC=0
if [ "$RUN" = "1" ]; then
    set +e
    timeout 300 "$BIN" "$VOCAB" "$TEMPLATES"
    RC=$?
    set -e
fi
T5=$(date +%s%N)

echo "FG571_BUILD opt=$OPT gen=$GS fixture=$FS goose_ms=$(ms $T1 $T0) gcc_ms=$(ms $T2 $T1) fixture_ms=$(ms $T3 $T2) link_ms=$(ms $T4 $T3) run_ms=$(ms $T5 $T4) binary=$BIN"
exit $RC
