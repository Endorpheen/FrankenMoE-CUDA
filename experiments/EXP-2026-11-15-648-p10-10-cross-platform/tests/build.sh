#!/usr/bin/env bash
# Differential gate build for the P10-10 cross-platform slices.
#
#   bash tests/build.sh [final|o0|all|mut]
#
# final  build every slice with the generated C at -O1 and run it
# o0     the same at -O0
# all    final, then o0, then diff the two outputs line by line (default)
# mut    build once at -O0 without the source cache, used by mutate.py
#
# Nothing here depends on the caller's working directory: the slice root is
# derived from this script's location and no absolute scratch path is stored.
# Overrides, all optional:
#   GOOSE_BIN, CC, CXX, FG_UPSTREAM, OUT_DIR, FG_ONLY (one slice name)
set -euo pipefail

HERE=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
SUB=$(cd "$HERE/.." && pwd)
GOOSE=${GOOSE_BIN:-/home/end0/AI/LlamaServer/toolchain/goose-2026.3-string/goose}
CC=${CC:-gcc-13}
CXX=${CXX:-g++-13}
UP=${FG_UPSTREAM:-/home/end0/AI/FrankenMoE-CUDA/upstream/llama.cpp}
OUT=${OUT_DIR:-$SUB/build}
MODE=${1:-all}
ONLY=${FG_ONLY:-}

mkdir -p "$OUT"

SLICES=(
    "feat feat-cpu-features.goose fixture_feat.cpp"
    "plat plat-paths.goose       fixture_plat.cpp"
    "dev  dev-backend-select.goose fixture_dev.cpp"
)

build_one() {
    local tag=$1 src=$2 fixture=$3 opt=$4
    local gsrc=$SUB/source/$src
    local gen=$OUT/$tag-gen.c
    local hdr=$OUT/$tag.h
    local obj=$OUT/$tag-${opt#-}.o
    local bin=$OUT/$tag-${opt#-}-gate
    local key
    key="$(sha256sum "$gsrc" | cut -d' ' -f1) $opt $($CC --version | head -1)"

    if [ "$MODE" = "mut" ] || [ ! -f "$obj" ] || [ "$(cat "$OUT/$tag-${opt#-}.key" 2>/dev/null || true)" != "$key" ]; then
        "$GOOSE" --standalone -o "$gen" --header "$hdr" "$gsrc" >/dev/null
        $CC "$opt" -std=c11 -w -c "$gen" -o "$obj"
        printf '%s\n' "$key" > "$OUT/$tag-${opt#-}.key"
    fi
    $CXX -O2 -std=c++17 -I "$OUT" -I "$UP/ggml/src" -c "$HERE/$fixture" -o "$OUT/$tag-fixture.o"
    $CXX "$OUT/$tag-fixture.o" "$obj" -o "$bin"
    "$bin"
}

run_mode() {
    local opt=$1
    local rc=0
    for s in "${SLICES[@]}"; do
        set -- $s
        if [ -n "$ONLY" ] && [ "$ONLY" != "$1" ]; then continue; fi
        local line
        line=$(build_one "$1" "$2" "$3" "$opt" 2>&1 | tail -1)
        echo "[$opt] $line"
        echo "$line" > "$OUT/$1-${opt#-}.out"
        case "$line" in
            *"0 failures"*) ;;
            *) rc=1 ;;
        esac
    done
    return $rc
}

case "$MODE" in
    final) run_mode -O1 ;;
    o0)    run_mode -O0 ;;
    mut)   run_mode -O0 ;;
    all)
        run_mode -O1
        run_mode -O0
        rc=0
        for s in "${SLICES[@]}"; do
            set -- $s
            if [ -n "$ONLY" ] && [ "$ONLY" != "$1" ]; then continue; fi
            if diff -u "$OUT/$1-O1.out" "$OUT/$1-O0.out" > "$OUT/$1-diff.txt"; then
                echo "[diff] $1: -O1 and -O0 identical"
            else
                echo "[diff] $1: MISMATCH"
                cat "$OUT/$1-diff.txt"
                rc=1
            fi
        done
        exit $rc
        ;;
    *) echo "usage: $0 [final|o0|all|mut]" >&2; exit 2 ;;
esac
