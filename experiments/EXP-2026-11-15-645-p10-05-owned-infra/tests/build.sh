#!/usr/bin/env bash
# P10-05 gate build: three owned-infrastructure slices, one script.
#
#   bash tests/build.sh                # all three gates at -O1
#   bash tests/build.sh pkg            # one gate
#   bash tests/build.sh pkg mutate     # one gate at -O0, for a mutation run
#
# Layout, derived from this script's location:
#   source/release-plan.goose               -> build/pkg.c -> gate-pkg
#   source/build-env.goose                  -> build/env.c -> gate-env
#   source/generated-artifact-policy.goose  -> build/art.c -> gate-art
#
# Native arms live in tests/oracle_*.py and execute the byte-identical copies in
# tests/native/; nothing here writes into the repository.
#
# Overrides: GOOSE, CC, CXX, FG_OUT.
set -euo pipefail

EXP="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
GOOSE=${GOOSE:-/home/end0/AI/LlamaServer/toolchain/goose-2026.3-string/goose}
CCB=${CC:-gcc-13}
CXXB=${CXX:-g++-13}
OUT=${FG_OUT:-$EXP/build}

SLICE=${1:-all}
MODE=${2:-final}
case "$MODE" in
    final) OPT=-O1 ;;
    mutate) OPT=-O0 ;;
    *) echo "usage: build.sh [pkg|env|art|all] [final|mutate]" >&2; exit 2 ;;
esac

mkdir -p "$OUT"
VER=$("$CCB" -dumpfullversion 2>/dev/null || echo unknown)

ms() { echo $(( ($1 - $2) / 1000000 )); }

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

run_pkg() {
    local t0 t1
    t0=$(date +%s%N)
    gen_c pkg "$EXP/source/release-plan.goose"
    mkdir -p "$OUT/pkgdist"
    "$CXXB" -O2 -std=c++17 -I"$OUT" "$EXP/tests/pkg_fixture.cpp" "$OUT/gen-pkg.o" -o "$OUT/gate-pkg"
    t1=$(date +%s%N)
    "$OUT/gate-pkg" "$EXP/tests/cases/pkg.tsv" "$OUT/pkgdist" "$EXP/tests/oracle_pkg.py"
    echo "build slice=pkg mode=$MODE opt=$OPT link_ms=$(ms "$t1" "$t0")"
}

run_env() {
    local t0 t1
    t0=$(date +%s%N)
    gen_c env "$EXP/source/build-env.goose"
    "$CXXB" -O2 -std=c++17 -I"$OUT" "$EXP/tests/env_fixture.cpp" "$OUT/gen-env.o" -o "$OUT/gate-env"
    t1=$(date +%s%N)
    ( cd "$OUT" && "$OUT/gate-env" "$EXP/tests/cases/env.tsv" "$EXP/tests/native" "$EXP/tests/oracle_env.py" "$OUT" )
    echo "build slice=env mode=$MODE opt=$OPT link_ms=$(ms "$t1" "$t0")"
}

run_art() {
    local t0 t1
    t0=$(date +%s%N)
    gen_c art "$EXP/source/generated-artifact-policy.goose"
    "$CXXB" -O2 -std=c++17 -I"$OUT" "$EXP/tests/art_fixture.cpp" "$OUT/gen-art.o" -o "$OUT/gate-art"
    t1=$(date +%s%N)
    "$OUT/gate-art" "$EXP/tests/cases/art.tsv" "$EXP/tests/oracle_art.py" "$OUT"
    echo "build slice=art mode=$MODE opt=$OPT link_ms=$(ms "$t1" "$t0")"
}

case "$SLICE" in
    pkg) run_pkg ;;
    env) run_env ;;
    art) run_art ;;
    all) run_pkg; run_env; run_art ;;
    *) echo "usage: build.sh [pkg|env|art|all] [final|mutate]" >&2; exit 2 ;;
esac
