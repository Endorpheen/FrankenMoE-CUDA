#!/usr/bin/env bash
# P9-01 gate build: three slices, one script.
#
#   bash tests/build.sh                # all three gates at -O1
#   bash tests/build.sh cfg            # one gate
#   bash tests/build.sh ar mutate      # one gate at -O0, for the mutation run
#
# Layout, all derived from this script's location:
#   <exp>/source/config-validate.goose   -> build/gen-cfg.c  -> gate-cfg
#   <exp>/source/arch-registry.goose     -> build/gen-ar.c   -> gate-ar
#   <exp>/source/config-resolve.goose    -> build/gen-cc.c   -> gate-cc
#
# Native arms are compiled from FG_CORE (the runtime/core tree of the base
# commit), so the config and registry gates compare against the real functions,
# not against copies. The CLI gate needs no native object: main.cpp cannot be
# linked without the engine and its oracle is a transcription inside the
# fixture.
#
# No absolute scratch paths. Overrides: FG_CORE, FG_GOOSE, GOOSE, CC, CXX.
set -euo pipefail

EXP="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
CORE=${FG_CORE:-/home/end0/AI/FrankenMoE-CUDA/runtime/core}
GOOSE=${FG_GOOSE:-${GOOSE:-/home/end0/AI/LlamaServer/toolchain/goose-2026.3-string/goose}}
CCB=${CC:-gcc-13}
CXXB=${CXX:-g++-13}
OUT=$EXP/build

SLICE=${1:-all}
MODE=${2:-final}
case "$MODE" in
    final) OPT=-O1 ;;
    mutate) OPT=-O0 ;;
    *) echo "usage: build.sh [cfg|ar|cc|all] [final|mutate]" >&2; exit 2 ;;
esac

mkdir -p "$OUT"

ms() { echo $(( ($1 - $2) / 1000000 )); }
VER=$("$CCB" -dumpfullversion 2>/dev/null || echo unknown)

native_config() {
    local o=$OUT/native-config.o
    if [ ! -f "$o" ] || [ "$CORE/src/config.cpp" -nt "$o" ]; then
        "$CXXB" -O2 -std=c++17 -I"$CORE/include" -c "$CORE/src/config.cpp" -o "$o"
    fi
    echo "$o"
}

native_arch() {
    local o=$OUT/native-arch.o
    if [ ! -f "$o" ] || [ "$CORE/src/moe/arch_registry.cpp" -nt "$o" ]; then
        "$CXXB" -O2 -std=c++17 -I"$CORE/include" -c "$CORE/src/moe/arch_registry.cpp" -o "$o"
    fi
    echo "$o"
}

# gen_c <slice> <source> <header>
gen_c() {
    local name=$1 src=$2 hdr=$3
    local c=$OUT/gen-$name.c
    local key
    key=$(sha256sum "$src" | awk '{print $1}')"-$OPT-$VER"
    if [ "$MODE" = mutate ] || [ ! -f "$OUT/gen-$name.o" ] || [ "$(cat "$OUT/.key-$name" 2>/dev/null || true)" != "$key" ]; then
        timeout 300 "$GOOSE" --standalone -o "$c" --header "$OUT/$hdr" "$src"
        "$CCB" "$OPT" -std=c11 -w -c "$c" -o "$OUT/gen-$name.o"
        printf '%s\n' "$key" > "$OUT/.key-$name"
    fi
}

run_cfg() {
    local t0 t1 t2
    t0=$(date +%s%N)
    gen_c cfg "$EXP/source/config-validate.goose" cfg.h
    local nat
    nat=$(native_config)
    t1=$(date +%s%N)
    "$CXXB" -O2 -std=c++20 -I"$OUT" -I"$CORE/include" \
        "$EXP/tests/config_fixture.cpp" "$OUT/gen-cfg.o" "$nat" -o "$OUT/gate-cfg"
    t2=$(date +%s%N)
    "$OUT/gate-cfg"
    echo "build slice=cfg mode=$MODE opt=$OPT gen_ms=$(ms "$t1" "$t0") link_ms=$(ms "$t2" "$t1")"
}

run_ar() {
    local t0 t1 t2
    t0=$(date +%s%N)
    gen_c ar "$EXP/source/arch-registry.goose" ar.h
    local nat
    nat=$(native_arch)
    t1=$(date +%s%N)
    "$CXXB" -O2 -std=c++20 -I"$OUT" -I"$CORE/include" \
        "$EXP/tests/arch_fixture.cpp" "$OUT/gen-ar.o" "$nat" -o "$OUT/gate-ar"
    t2=$(date +%s%N)
    "$OUT/gate-ar"
    echo "build slice=ar mode=$MODE opt=$OPT gen_ms=$(ms "$t1" "$t0") link_ms=$(ms "$t2" "$t1")"
}

run_cc() {
    local t0 t1
    t0=$(date +%s%N)
    gen_c cc "$EXP/source/config-resolve.goose" cc.h
    t1=$(date +%s%N)
    "$CXXB" -O2 -std=c++20 -I"$OUT" "$EXP/tests/config_resolve_fixture.cpp" \
        "$OUT/gen-cc.o" -o "$OUT/gate-cc"
    "$OUT/gate-cc"
    echo "build slice=cc mode=$MODE opt=$OPT gen_ms=$(ms "$t1" "$t0")"
}

case "$SLICE" in
    cfg) run_cfg ;;
    ar) run_ar ;;
    cc) run_cc ;;
    all) run_cfg; run_ar; run_cc ;;
    *) echo "usage: build.sh [cfg|ar|cc|all] [final|mutate]" >&2; exit 2 ;;
esac
