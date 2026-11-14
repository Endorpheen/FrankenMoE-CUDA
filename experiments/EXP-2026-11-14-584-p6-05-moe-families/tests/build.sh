#!/bin/bash
# Structure gate for the three MoE families (P6-05 architectures 4, 7, 13).
#
#   tests/build.sh                 # all three families, one after another
#   tests/build.sh qwen3moe        # one family only
#   tests/build.sh qwen2moe
#   tests/build.sh glm4-moe
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
    local name=$1 tag=$2 src=$3 fixture=$4 short=$5
    local gen_c=$OUT/gen-$short.c gen_h=$OUT/runtime-gen-$short.h
    local gen_o=$OUT/gen-$short.o fix_o=$OUT/fixture-$short.o bin=$OUT/fg581-$short
    local need_gen=0

    if [ ! -f "$gen_o" ] || [ ! -f "$gen_h" ]; then
        need_gen=1
    elif [ "$S/source/$src" -nt "$gen_o" ] || [ "$S/source/base/model_graph.goose" -nt "$gen_o" ]; then
        need_gen=1
    elif [ "$(cat "$OUT/.opt-$short" 2>/dev/null || true)" != "$OPT" ]; then
        need_gen=1
    fi

    if [ "$need_gen" = 1 ]; then
        echo "[fg581] $name goose --standalone"
        "$G" --standalone -o "$gen_c" --header "$gen_h" "$S/source/$src"
        echo "[fg581] $name gcc $OPT"
        "$CC" "$OPT" -std=c11 -c "$gen_c" -o "$gen_o"
        echo "$OPT" > "$OUT/.opt-$short"
    else
        echo "[fg581] $name gen.o cached"
    fi

    echo "[fg581] $name g++ fixture"
    "$CXX" "$OPT" -std=c++17 -I"$OUT" -I"$S/tests" -c "$S/tests/$fixture" -o "$fix_o"

    echo "[fg581] $name link"
    "$CXX" "$fix_o" "$gen_o" -o "$bin"

    echo "[fg581] $name run"
    "$bin"
}

mkdir -p "$OUT"

case "${1:-all}" in
    qwen3moe) build_one qwen3moe FG-ARCH4 qwen3moe.goose fixture_qwen3moe.cpp q3m ;;
    qwen2moe) build_one qwen2moe FG-ARCH7 qwen2moe.goose fixture_qwen2moe.cpp q2m ;;
    glm4-moe) build_one glm4-moe FG-ARCH13 glm4-moe.goose fixture_glm4moe.cpp g4m ;;
    all)
        build_one qwen3moe FG-ARCH4 qwen3moe.goose fixture_qwen3moe.cpp q3m
        build_one qwen2moe FG-ARCH7 qwen2moe.goose fixture_qwen2moe.cpp q2m
        build_one glm4-moe FG-ARCH13 glm4-moe.goose fixture_glm4moe.cpp g4m
        ;;
    *) echo "usage: $0 [qwen3moe|qwen2moe|glm4-moe]" >&2; exit 2 ;;
esac
