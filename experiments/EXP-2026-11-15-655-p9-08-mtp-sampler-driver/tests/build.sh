#!/usr/bin/env bash
# P9-08 G4/G5/G7 gate build: three residual-slice gates.
#
#   bash tests/build.sh              # all three gates at -O1
#   bash tests/build.sh drv          # one gate
#   bash tests/build.sh mw mutate    # one gate at -O0, for a mutation run
#
# Layout, derived from this script's location:
#   tests/native_extract.py pulls the verbatim native regions out of the base
#   commit into build/native_*.inc; each gate then compiles the extracted
#   carrier code against the composed Goose unit.
#
# No absolute scratch paths inside the experiment. Overrides: FG_REPO, FG_GOOSE,
# CC, CXX.
set -euo pipefail

EXP="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
REPO=${FG_REPO:-/home/end0/AI/FrankenMoE-CUDA}
GOOSE=${FG_GOOSE:-${GOOSE:-/home/end0/AI/LlamaServer/toolchain/goose-2026.3-string/goose}}
CCB=${CC:-gcc-13}
CXXB=${CXX:-g++-13}
OUT=$EXP/build

SLICE=${1:-all}
MODE=${2:-final}
case "$MODE" in
    final) OPT=-O1 ;;
    mutate) OPT=-O0 ;;
    *) echo "usage: build.sh [sch|mw|drv|all] [final|mutate]" >&2; exit 2 ;;
esac

mkdir -p "$OUT"

VER=$("$CXXB" -dumpfullversion 2>/dev/null || echo unknown)

python3 "$EXP/tests/native_extract.py" "$REPO" "$OUT" >/dev/null

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

gate() {
    local name=$1 fixture=$2
    "$CXXB" -O2 -std=c++17 -I"$OUT" -I"$REPO/runtime/core/include" "$EXP/tests/$fixture" "$OUT/gen-$name.o" -lm -o "$OUT/gate-$name"
    "$OUT/gate-$name"
    echo "build slice=$name mode=$MODE opt=$OPT"
}

run_sch() { gen_c sch "$EXP/source/sampler-chain.goose";          gate sch sch_fixture.cpp; }
run_mw()  { gen_c mw  "$EXP/source/mtp-capture-warmup.goose";     gate mw  mw_fixture.cpp; }
run_drv() { gen_c drv "$EXP/source/cli-driver-loop.goose";        gate drv drv_fixture.cpp; }

case "$SLICE" in
    sch) run_sch ;;
    mw) run_mw ;;
    drv) run_drv ;;
    all) run_sch; run_mw; run_drv ;;
    *) echo "usage: build.sh [sch|mw|drv|all] [final|mutate]" >&2; exit 2 ;;
esac
