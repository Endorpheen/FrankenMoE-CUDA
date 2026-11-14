#!/bin/bash
# Differential gates for P7-12: GGUF/loader/tokenizer/common decisions that can
# be checked without a model file.
#
#   tests/build.sh              build and run all three gates at -O1
#   tests/build.sh dispatch     just S1 (vocab type and pre-tokenizer dispatch)
#   tests/build.sh partition    just S2 (special-token partitioning)
#   tests/build.sh strings      just S3 (common string helpers)
#
# Environment, all optional:
#   LLAMA_DIR   native tree (default: the llama.cpp-flint-cuda checkout)
#   GOOSE_BIN   the pinned goose binary
#   GOOSE_SRC   alternate goose source, used by tests/mutate.py
#   OUT_DIR     build directory
#   OPT         optimisation flag; the gate runs at -O1, mutations at -O0
#   GATE_ARG    extra argument for the gate binary ("--quick" for a smoke run)
#
# The experiment root is derived from this script, so no absolute temporary
# path is baked in. libllama-common, libllama and libggml-base are used as the
# native oracle for S3 and are loaded from the native build directory.
set -euo pipefail

S=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
L=${LLAMA_DIR:-/home/end0/AI/LlamaServer/llama.cpp-flint-cuda}
G=${GOOSE_BIN:-/home/end0/AI/LlamaServer/toolchain/goose-2026.3-string/goose}
CC=${CC:-gcc-13}
CXX=${CXX:-g++-13}
OUT=${OUT_DIR:-$S/build}
OPT=${OPT:--O1}
WHAT=${1:-all}

mkdir -p "$OUT"
OUT=$(cd "$OUT" && pwd)

cflags_native=(-I"$L/include" -I"$L/ggml/include")
cflags_gate=(-I"$OUT" -I"$L/src" -I"$L/include" -I"$L/ggml/include")

# The oracle sources are cut out of the native tree on every build: the gate
# never uses a hand transcription.
if [ ! -f "$OUT/native-dispatch.cpp" ] || [ ! -f "$OUT/native-partition.cpp" ]; then
    echo "[p712] extracting the native oracle bodies"
    python3 "$S/tests/make-native.py" "$L/src/llama-vocab.cpp" \
        "$L/src/llama-vocab.h" "$L/include/llama.h" "$OUT"
fi

build_native() {
    local name=$1
    "$CXX" "$OPT" -std=c++17 "${cflags_gate[@]}" -c "$OUT/native-$name.cpp" \
        -o "$OUT/native-$name.o"
}

build_port() {
    local src=$1 gen=$2
    local out_src=${GOOSE_SRC:-$src}
    "$G" --standalone -o "$OUT/$gen.c" --header "$OUT/$gen.h" "$out_src" >/dev/null
    "$CC" "$OPT" -std=c11 -c "$OUT/$gen.c" -o "$OUT/$gen.o"
}

run_dispatch() {
    build_native dispatch
    build_port "$S/source/vocab-type.goose" vocab-type
    [ -f "$OUT/dispatch-cases.tsv" ] || python3 "$S/tests/gen-cases.py" \
        "$L/src/llama-vocab.cpp" "$OUT/dispatch-cases.tsv"
    "$CXX" "$OPT" -std=c++17 "${cflags_gate[@]}" "$S/tests/fixture_dispatch.cpp" \
        "$OUT/vocab-type.o" "$OUT/native-dispatch.o" -o "$OUT/gate-dispatch"
    if [ "${GATE_ARG:-}" = "--quick" ]; then
        ( cd "$OUT" && ./gate-dispatch --quick )
    else
        ( cd "$OUT" && ./gate-dispatch "$OUT/dispatch-cases.tsv" )
    fi
}

run_partition() {
    build_native partition
    build_port "$S/source/special-partition.goose" special-partition
    [ -f "$OUT/partition-cases.tsv" ] || python3 "$S/tests/gen-partition-cases.py" \
        "$OUT/partition-cases.tsv"
    "$CXX" "$OPT" -std=c++17 -include algorithm "${cflags_gate[@]}" \
        "$S/tests/fixture_partition.cpp" "$OUT/special-partition.o" \
        "$OUT/native-partition.o" -o "$OUT/gate-partition"
    ( cd "$OUT" && ./gate-partition "$OUT/partition-cases.tsv" )
}

run_strings() {
    build_port "$S/source/common-strings.goose" common-strings
    "$CXX" "$OPT" -std=c++17 "${cflags_native[@]}" -I"$OUT" \
        "$S/tests/fixture_strings.cpp" "$OUT/common-strings.o" \
        -L"$L/build-cuda/bin" -lllama-common -lllama -lggml-base \
        -Wl,-rpath,"$L/build-cuda/bin" -o "$OUT/gate-strings"
    "$OUT"/gate-strings 2>"$OUT/strings-native.log"
}

case "$WHAT" in
    dispatch)  run_dispatch ;;
    partition) run_partition ;;
    strings)   run_strings ;;
    all)
        run_dispatch
        run_partition
        run_strings
        ;;
    *) echo "usage: $0 [dispatch|partition|strings|all]" >&2; exit 2 ;;
esac
