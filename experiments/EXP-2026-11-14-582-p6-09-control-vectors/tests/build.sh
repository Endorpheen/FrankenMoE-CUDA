#!/bin/bash
# Build and run the P6-09 S3 control-vector gate (EXP578).
#
# Usage: tests/build.sh [--no-run]
#
# Inputs: source/cvec.goose, tests/fixture.cpp.
# Steps: goose --standalone -> runtime-gen.c/.h -> gcc -O0 -> g++ fixture ->
# link libllama/libggml -> run with a CPU-backed direction table. gen.o and
# fixture.o are cached by timestamp.
#
# The fixture links the real adapter methods. llama_model is abstract in this
# tree, so the fixture uses TestModel, an hparams-only subclass whose pure
# virtuals are unused stubs (see tests/README.md).
#
# Environment overrides:
#   FG578_SRC    Goose source        (default <root>/source/cvec.goose)
#   FG578_OUT    build directory     (default <root>/build)
#   FG578_LLAMA  flint llama.cpp     (default /home/end0/AI/LlamaServer/llama.cpp-flint-cuda)
#   FG578_LAYERS gate layer count    (default 32)
#   FG578_EMBD   gate embedding size (default 4096)
#   FG578_GOOSE  goose binary        (default /home/end0/AI/Fried-Goose/build/goose)
#   FG578_OPT    C/C++ optimization  (default -O0)
#   FG578_CC     C compiler          (default gcc-13)
#   FG578_CXX    C++ compiler        (default g++-13)

set -e

HERE=$(cd "$(dirname "$0")" && pwd)
ROOT=$(cd "$HERE/.." && pwd)

SRC=${FG578_SRC:-$ROOT/source/cvec.goose}
FIXTURE=$HERE/fixture.cpp
OUT=${FG578_OUT:-$ROOT/build}
L=${FG578_LLAMA:-/home/end0/AI/LlamaServer/llama.cpp-flint-cuda}
BIN_DIR=$L/build-cuda/bin
LAYERS=${FG578_LAYERS:-32}
EMBD=${FG578_EMBD:-4096}
G=${FG578_GOOSE:-/home/end0/AI/Fried-Goose/build/goose}
OPT=${FG578_OPT:--O0}
CC=${FG578_CC:-gcc-13}
CXX=${FG578_CXX:-g++-13}

RUN=1
if [ "${1:-}" = "--no-run" ]; then
    RUN=0
fi

for f in "$SRC" "$FIXTURE" "$G"; do
    if [ ! -f "$f" ]; then
        echo "FG578_BUILD missing input: $f" >&2
        exit 1
    fi
done
if [ ! -d "$BIN_DIR" ]; then
    echo "FG578_BUILD missing llama build: $BIN_DIR" >&2
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
        -I"$OUT" -I"$L/src" -I"$L/common" -I"$L/include" -I"$L/ggml/include" \
        -c "$FIXTURE" -o "$FIX_O"
    FS=rebuilt
else
    FS=cached
fi
T3=$(date +%s%N)

BIN=$OUT/fg578-cv
timeout 300 "$CXX" "$FIX_O" "$GEN_O" \
    -L"$BIN_DIR" -lllama -lggml -lggml-base -lm \
    -Wl,-rpath,"$BIN_DIR" -o "$BIN"
T4=$(date +%s%N)

RC=0
if [ "$RUN" = "1" ]; then
    set +e
    timeout 300 "$BIN" "$LAYERS" "$EMBD"
    RC=$?
    set -e
fi
T5=$(date +%s%N)

echo "FG578_BUILD opt=$OPT gen=$GS fixture=$FS goose_ms=$(ms $T1 $T0) gcc_ms=$(ms $T2 $T1) fixture_ms=$(ms $T3 $T2) link_ms=$(ms $T4 $T3) run_ms=$(ms $T5 $T4) binary=$BIN"
exit $RC
