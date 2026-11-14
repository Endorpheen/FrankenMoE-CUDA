#!/bin/bash
# Structure gates for the qwen3 and qwen2 builders (P6-05 architectures 3, 6).
#
#   tests/build.sh
#
# Two independent programs: the modules export the same helper names, so they
# are never linked together. No native library is linked; the expectations are
# a transcription of src/models/qwen3.cpp and src/models/qwen2.cpp.
#
# The root is derived from this script's location, so no absolute paths are
# stored. The gate runs at -O1; mutation builds override OPT=-O0.
set -uo pipefail

S=$(cd "$(dirname "$0")/.." && pwd)
G=${GOOSE_BIN:-/home/end0/AI/LlamaServer/toolchain/goose-2026.3-string/goose}
CC=${CC:-gcc-13}
CXX=${CXX:-g++-13}
OPT=${OPT:--O1}
OUT=${OUT_DIR:-$S/build}

mkdir -p "$OUT"

gen_stale() {
    local o=$1 tag=$2 src=$3
    if [ ! -f "$o" ] || [ ! -f "$OUT/runtime-$tag.h" ]; then
        return 0
    fi
    if [ "$src" -nt "$o" ] || [ "$S/source/base/dense.goose" -nt "$o" ] \
       || [ "$S/source/base/model_graph.goose" -nt "$o" ]; then
        return 0
    fi
    if [ "$(cat "$OUT/.opt-$tag" 2>/dev/null || true)" != "$OPT" ]; then
        return 0
    fi
    return 1
}

build_one() {
    local tag=$1 family=$2 src=$3 fixture=$4
    local bin=$OUT/fg-$tag

    if gen_stale "$OUT/gen-$tag.o" "$tag" "$src"; then
        echo "[fg-$tag] goose --standalone"
        "$G" --standalone -o "$OUT/runtime-$tag.c" --header "$OUT/runtime-$tag.h" "$src" || return 1
        echo "[fg-$tag] $CC $OPT"
        "$CC" "$OPT" -std=c11 -c "$OUT/runtime-$tag.c" -o "$OUT/gen-$tag.o" || return 1
        echo "$OPT" > "$OUT/.opt-$tag"
    else
        echo "[fg-$tag] gen.o cached"
    fi

    echo "[fg-$tag] g++ fixture"
    "$CXX" "$OPT" -std=c++17 -I"$OUT" -c "$fixture" -o "$OUT/fixture-$tag.o" || return 1
    echo "[fg-$tag] link"
    "$CXX" "$OUT/fixture-$tag.o" "$OUT/gen-$tag.o" -o "$bin" || return 1

    echo "[fg-$tag] run $family"
    "$bin"
}

rc3=0
rc6=0
build_one arch3 qwen3 "$S/source/qwen3.goose" "$S/tests/fixture_arch3.cpp" || rc3=1
build_one arch6 qwen2 "$S/source/qwen2.goose" "$S/tests/fixture_arch6.cpp" || rc6=1

if [ "$rc3" != 0 ] || [ "$rc6" != 0 ]; then
    echo "[fg] gate failure: arch3=$rc3 arch6=$rc6"
    exit 1
fi
