#!/bin/bash
# Structure gate for the three Gemma 4 / Mistral 3 builders
# (P6-05 architectures 18, 19, 20).
#
#   tests/build.sh                  # all three families, one after another
#   tests/build.sh gemma4           # architecture 18
#   tests/build.sh gemma4-assistant # architecture 19
#   tests/build.sh mistral3         # architecture 20
#
# No native library: the drafts are pure plan/shape decisions. The root is
# derived from this script's location, so no absolute paths are stored.
set -euo pipefail

S=$(cd "$(dirname "$0")/.." && pwd)
G=${GOOSE_BIN:-/home/end0/AI/LlamaServer/toolchain/goose-2026.3-string/goose}
CC=${CC:-gcc-13}
CXX=${CXX:-g++-13}
OPT=${OPT:--O1}
OUT=${OUT_DIR:-$S/build}

build_one() {
    local name=$1 src=$2 fixture=$3 short=$4
    local gen_c=$OUT/gen-$short.c gen_h=$OUT/runtime-gen-$short.h
    local gen_o=$OUT/gen-$short.o fix_o=$OUT/fixture-$short.o bin=$OUT/fg-g4m3-$short
    local need_gen=0

    if [ ! -f "$gen_o" ] || [ ! -f "$gen_h" ]; then
        need_gen=1
    elif [ "$S/source/$src" -nt "$gen_o" ] || [ "$S/source/base/model_graph.goose" -nt "$gen_o" ]; then
        need_gen=1
    elif [ "$(cat "$OUT/.opt-$short" 2>/dev/null || true)" != "$OPT" ]; then
        need_gen=1
    fi

    if [ "$need_gen" = 1 ]; then
        echo "[g4m3] $name goose --standalone"
        "$G" --standalone -o "$gen_c" --header "$gen_h" "$S/source/$src"
        echo "[g4m3] $name gcc $OPT"
        "$CC" "$OPT" -std=c11 -c "$gen_c" -o "$gen_o"
        echo "$OPT" > "$OUT/.opt-$short"
    else
        echo "[g4m3] $name gen.o cached"
    fi

    echo "[g4m3] $name g++ fixture"
    "$CXX" "$OPT" -std=c++17 -I"$OUT" -I"$S/tests" -c "$S/tests/$fixture" -o "$fix_o"

    echo "[g4m3] $name link"
    "$CXX" "$fix_o" "$gen_o" -o "$bin"

    echo "[g4m3] $name run"
    "$bin"
}

mkdir -p "$OUT"

case "${1:-all}" in
    gemma4)           build_one gemma4           gemma4.goose           fixture_gemma4.cpp     g4 ;;
    gemma4-assistant) build_one gemma4-assistant gemma4-assistant.goose fixture_g4assistant.cpp g4a ;;
    mistral3)         build_one mistral3         mistral3.goose         fixture_mistral3.cpp   m3 ;;
    all)
        build_one gemma4           gemma4.goose           fixture_gemma4.cpp     g4
        build_one gemma4-assistant gemma4-assistant.goose fixture_g4assistant.cpp g4a
        build_one mistral3         mistral3.goose         fixture_mistral3.cpp   m3
        ;;
    *) echo "usage: $0 [gemma4|gemma4-assistant|mistral3]" >&2; exit 2 ;;
esac
