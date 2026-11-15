#!/usr/bin/env bash
# P10-07 gate build: three vendored-dependency slices, one script.
#
#   bash tests/build.sh                # all three gates at -O1
#   bash tests/build.sh vnd            # one gate
#   bash tests/build.sh vnd mutate     # one gate at -O0, for a mutation run
#
# Layout, derived from this script's location:
#   source/vendor-licence-payload.goose -> build/lic.c -> gate-lic
#   source/vendor-provenance.goose      -> build/vnd.c -> gate-vnd
#   source/vendor-selection.goose       -> build/sys.c -> gate-sys
#
# Native arms live in tests/oracle_*.py and execute or parse the byte-identical
# copies in tests/native/; nothing here writes into the repository.
#
# Overrides: GOOSE, CC, CXX, FG_OUT, FG_LLAMA (the pinned llama.cpp tree, for
# the vendor table and the real vendor directory).
set -euo pipefail

EXP="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
GOOSE=${GOOSE:-/home/end0/AI/LlamaServer/toolchain/goose-2026.3-string/goose}
CCB=${CC:-gcc-13}
CXXB=${CXX:-g++-13}
OUT=${FG_OUT:-$EXP/build}
LLAMA=${FG_LLAMA:-$(cd "$EXP/../../../.." 2>/dev/null && pwd)/upstream/llama.cpp}
if [ ! -d "$LLAMA/vendor" ]; then
    LLAMA=/home/end0/AI/FrankenMoE-CUDA/upstream/llama.cpp
fi

SLICE=${1:-all}
MODE=${2:-final}
case "$MODE" in
    final) OPT=-O1 ;;
    mutate) OPT=-O0 ;;
    *) echo "usage: build.sh [lic|vnd|sys|all] [final|mutate]" >&2; exit 2 ;;
esac

mkdir -p "$OUT"
VER=$("$CCB" -dumpfullversion 2>/dev/null || echo unknown)

gen_c() {
    local name=$1 src=$2
    local key
    key=$(sha256sum "$src" | awk '{print $1}')"-$OPT-$VER"
    if [ "$MODE" = mutate ] || [ ! -f "$OUT/gen-$name.o" ] || [ "$(cat "$OUT/.key-$name" 2>/dev/null || true)" != "$key" ]; then
        timeout 300 "$GOOSE" --standalone -o "$OUT/$name.c" --header "$OUT/$name.h" "$src" >/dev/null
        "$CCB" "$OPT" -std=c11 -w -c "$OUT/$name.c" -o "$OUT/gen-$name.o"
        printf '%s\n' "$key" > "$OUT/.key-$name"
    fi
}

run_lic() {
    gen_c lic "$EXP/source/vendor-licence-payload.goose"
    "$CXXB" -O2 -std=c++17 -I"$OUT" "$EXP/tests/lic_fixture.cpp" "$OUT/gen-lic.o" -o "$OUT/gate-lic"
    "$OUT/gate-lic" "$EXP/tests/cases/lic.tsv" "$EXP/tests/oracle_lic.py" \
        "$EXP/tests/native/license.cmake" "$EXP/tests/fixtures" "$OUT"
    echo "build slice=lic mode=$MODE opt=$OPT"
}

run_vnd() {
    gen_c vnd "$EXP/source/vendor-provenance.goose"
    "$CXXB" -O2 -std=c++17 -I"$OUT" "$EXP/tests/vnd_fixture.cpp" "$OUT/gen-vnd.o" -o "$OUT/gate-vnd"
    "$OUT/gate-vnd" "$EXP/tests/cases/vnd.tsv" "$EXP/tests/oracle_vnd.py" \
        "$EXP/tests/native/sync_vendor.py" "$EXP/tests/fixtures" "$LLAMA" "$OUT"
    echo "build slice=vnd mode=$MODE opt=$OPT"
}

run_sys() {
    gen_c sys "$EXP/source/vendor-selection.goose"
    "$CXXB" -O2 -std=c++17 -I"$OUT" "$EXP/tests/sys_fixture.cpp" "$OUT/gen-sys.o" -o "$OUT/gate-sys"
    "$OUT/gate-sys" "$EXP/tests/cases/sys.tsv" "$EXP/tests/oracle_sys.py" \
        "$EXP/tests/native" "$OUT"
    echo "build slice=sys mode=$MODE opt=$OPT"
}

case "$SLICE" in
    lic) run_lic ;;
    vnd) run_vnd ;;
    sys) run_sys ;;
    all) run_lic; run_vnd; run_sys ;;
    *) echo "usage: build.sh [lic|vnd|sys|all] [final|mutate]" >&2; exit 2 ;;
esac
