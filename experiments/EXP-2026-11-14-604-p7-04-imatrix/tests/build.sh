#!/bin/bash
# Differential gate for P7-04 S2: imatrix loader, remap and prepare.
#
#   tests/build.sh                                  regenerate, build both sides, diff
#   GOOSE_SRC=... OUT_DIR=... OPT=-O0 GOOSE_ONLY=1 tests/build.sh
#                                                   one mutant, diffed against build/native.out
#
# The native side needs the llama libraries of the base commit; tests/build.sh
# configures and builds them into build/llama.cpp on the first run. Nothing in
# the repository is modified.
set -euo pipefail

S=$(cd "$(dirname "$0")/.." && pwd)
REPO=${REPO:-/home/end0/AI/FrankenMoE-CUDA}
LLAMA_SRC=${LLAMA_SRC:-$REPO/upstream/llama.cpp}
LLAMA_BUILD=${LLAMA_BUILD:-$S/build/llama.cpp}
G=${GOOSE_BIN:-/home/end0/AI/LlamaServer/toolchain/goose-2026.3-string/goose}
CXX=${CXX:-g++-13}
OPT=${OPT:--O1}
OUT=${OUT_DIR:-$S/build}
GOOSE_SRC=${GOOSE_SRC:-$S/source/imatrix.goose}

mkdir -p "$OUT"

echo "[s2] fixtures -> tests/probe.cpp, $OUT/gt.goose"
GOOSE_SRC="$GOOSE_SRC" OUT_DIR="$OUT" python3 "$S/tests/gen_imx.py"

if [ "${GOOSE_ONLY:-0}" != "1" ]; then
    if [ ! -f "$LLAMA_BUILD/bin/libllama-common.so" ]; then
        echo "[s2] configure llama.cpp"
        cmake -S "$LLAMA_SRC" -B "$LLAMA_BUILD" -G Ninja \
            -DCMAKE_BUILD_TYPE=Release -DGGML_CUDA=OFF -DLLAMA_CURL=OFF \
            -DLLAMA_BUILD_TESTS=OFF -DLLAMA_BUILD_EXAMPLES=OFF \
            -DLLAMA_BUILD_TOOLS=OFF -DLLAMA_BUILD_SERVER=OFF \
            -DBUILD_SHARED_LIBS=ON > "$OUT/llama-configure.log" 2>&1
        echo "[s2] build llama.cpp"
        ninja -C "$LLAMA_BUILD" llama llama-common ggml-base > "$OUT/llama-build.log" 2>&1
    fi

    INC="-I$S/tests -I$LLAMA_BUILD -I$LLAMA_SRC/src -I$LLAMA_SRC/include -I$LLAMA_SRC/ggml/include -I$LLAMA_SRC/common -I$LLAMA_SRC/tools/quantize"

    echo "[s2] native oracle"
    "$CXX" "$OPT" -std=c++17 $INC -c "$S/tests/native_im.cpp"    -o "$OUT/native_im.o"
    "$CXX" "$OPT" -std=c++17 $INC -c "$S/tests/native_remap.cpp" -o "$OUT/native_remap.o"
    "$CXX" "$OPT" -std=c++17 $INC -c "$S/tests/probe.cpp"        -o "$OUT/probe.o"
    "$CXX" "$OUT/probe.o" "$OUT/native_im.o" "$OUT/native_remap.o" \
        -L"$LLAMA_BUILD/bin" -lllama -lllama-common -lggml-base -lggml -lm \
        -Wl,-rpath,"$LLAMA_BUILD/bin" -o "$OUT/probe"

    echo "[s2] run native oracle"
    "$OUT/probe" > "$S/build/native.out" 2> "$OUT/probe.err"
fi

echo "[s2] goose driver $OPT"
"$G" run "$OUT/gt.goose" "$OPT" > "$OUT/goose.out" 2> "$OUT/goose.err" || {
    echo "[s2] goose run failed"; tail -5 "$OUT/goose.err"; exit 1;
}

if [ "${GOOSE_ONLY:-0}" != "1" ]; then
    echo "[s2] diff"
    diff -u "$S/build/native.out" "$OUT/goose.out" | head -40 || true
    if ! diff -q "$S/build/native.out" "$OUT/goose.out" > /dev/null; then
        echo "[s2] GATE FAILED"
        exit 1
    fi
    echo "[s2] gate ok: $(wc -l < "$S/build/native.out") lines identical"
else
    if ! diff -q "$S/build/native.out" "$OUT/goose.out" > /dev/null; then
        echo "[s2] mutant differs from native"
        exit 1
    fi
    echo "[s2] mutant identical"
fi
