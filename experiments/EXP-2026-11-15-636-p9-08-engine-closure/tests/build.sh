#!/usr/bin/env bash
# Differential gate build for P9-08: standalone engine closure.
#
#   tests/build.sh [lga|ppl|kvp|all]
#
# For each slice: goose --standalone -> C -> gcc, then the fixture -> g++,
# then run. Nothing depends on the caller's working directory: the experiment
# root is derived from this script's location. Overrides, all optional:
#   GOOSE_BIN, CC, CXX, OPT (the gate runs at -O1), GOOSE_SRC, OUT_DIR, FG_ONLY
set -euo pipefail

HERE=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
SUB=$(cd "$HERE/.." && pwd)
GOOSE=${GOOSE_BIN:-/home/end0/AI/LlamaServer/toolchain/goose-2026.3-string/goose}
CC=${CC:-gcc-13}
CXX=${CXX:-g++-13}
OPT=${OPT:- -O1}
OPT=${OPT# }
OUT=${OUT_DIR:-$SUB/build}
WHICH=${1:-${FG_ONLY:-all}}

mkdir -p "$OUT"

build_one() {
    local tag=$1 src=$2 fixture=$3
    local gsrc=${GOOSE_SRC:-$SUB/source/$src}
    local obj=$OUT/$tag.o
    local gen=$OUT/$tag.c
    local hdr=$OUT/$tag.h
    local sha
    sha=$(sha256sum "$gsrc" | awk '{print $1}')

    # The cache key is the source SHA, the optimisation flag and the compiler.
    local key="$sha $OPT $($CC --version | head -1)"
    local stamp=$OUT/$tag.stamp
    if [ ! -f "$obj" ] || [ ! -f "$hdr" ] || [ "$(cat "$stamp" 2>/dev/null || true)" != "$key" ]; then
        echo "[fg634] $tag: goose --standalone ($gsrc)"
        "$GOOSE" --standalone -o "$gen" --header "$hdr" "$gsrc"
        "$CC" $OPT -std=c11 -w -c "$gen" -o "$obj"
        printf '%s\n' "$key" > "$stamp"
    else
        echo "[fg634] $tag: gen.o cached"
    fi

    echo "[fg634] $tag: g++ $fixture ($OPT)"
    "$CXX" $OPT -std=c++17 -I"$OUT" "$HERE/$fixture" "$obj" -lm -o "$OUT/$tag-gate"
    "$OUT/$tag-gate"
}

case "$WHICH" in
    lga) build_one lga lga-load-admission.goose fixture_lga.cpp ;;
    ppl) build_one ppl ppl-perplexity-plan.goose fixture_ppl.cpp ;;
    kvp) build_one kvp kvp-kv-turn-state.goose   fixture_kvp.cpp ;;
    all)
        build_one lga lga-load-admission.goose fixture_lga.cpp
        build_one ppl ppl-perplexity-plan.goose fixture_ppl.cpp
        build_one kvp kvp-kv-turn-state.goose   fixture_kvp.cpp
        ;;
    *) echo "usage: $0 [lga|ppl|kvp|all]" >&2; exit 2 ;;
esac
