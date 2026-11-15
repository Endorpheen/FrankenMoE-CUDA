#!/usr/bin/env bash
# P10-12 tallies/prefill/trace/moe-predict gate build: four residual-slice gates.
#
#   bash tests/build.sh              # all four gates at -O1
#   bash tests/build.sh tl           # one gate
#   bash tests/build.sh tr mutate    # one gate at -O0, for a mutation run
#
# Layout, derived from this script's location:
#   tests/native_extract.py pulls the verbatim native regions out of the base
#   commit into build/native_*.{h,inc}; each gate then links the extracted
#   carrier code against the composed Goose unit.
#
# No absolute scratch paths inside the experiment. Overrides: FG_REPO,
# FG_GOOSE, CC, CXX.
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
    *) echo "usage: build.sh [tl|pf|tr|mp|all] [final|mutate]" >&2; exit 2 ;;
esac

mkdir -p "$OUT"

ms() { echo $(( ($1 - $2) / 1000000 )); }
VER=$("$CCB" -dumpfullversion 2>/dev/null || echo unknown)

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
    local t0 t1
    t0=$(date +%s%N)
    "$CXXB" -O2 -std=c++17 -I"$OUT" -I"$EXP/tests" -I"$REPO/runtime/core/include" "$EXP/tests/$fixture" "$OUT/gen-$name.o" -lm -o "$OUT/gate-$name"
    t1=$(date +%s%N)
    "$OUT/gate-$name"
    echo "build slice=$name mode=$MODE opt=$OPT link_ms=$(ms "$t1" "$t0")"
}

run_tl() { gen_c tl "$EXP/source/tallies.goose";        gate tl tl_fixture.cpp; }
run_pf() { gen_c pf "$EXP/source/prefill-plan.goose";    gate pf pf_fixture.cpp; }
run_tr() { gen_c tr "$EXP/source/trace-frame-order.goose"; gate tr tr_fixture.cpp; }
run_mp() { gen_c mp "$EXP/source/moe-predict-report.goose"; gate mp mp_fixture.cpp; }

case "$SLICE" in
    tl) run_tl ;;
    pf) run_pf ;;
    tr) run_tr ;;
    mp) run_mp ;;
    all) run_tl; run_pf; run_tr; run_mp ;;
    *) echo "usage: build.sh [tl|pf|tr|mp|all] [final|mutate]" >&2; exit 2 ;;
esac
