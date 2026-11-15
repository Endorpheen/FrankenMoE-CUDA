#!/usr/bin/env bash
# P10-02 gate build: three CLI-tool slices, one script.
#
#   bash tests/build.sh                 # all three gates at -O1
#   bash tests/build.sh lb              # one gate
#   bash tests/build.sh tz mutate       # one gate at -O0, for a mutation run
#
# Layout, derived from this script's location:
#   source/llama-bench-report.goose -> build/lb.c -> gate-lb (native arm native_lb.o)
#   source/tokenize-cli.goose       -> build/tz.c -> gate-tz
#   source/fit-params-line.goose    -> build/fp.c -> gate-fp
#
# The native arm of the llama-bench gate is the tool's own translation unit from
# FG_NATIVE (the base-commit canon), included by tests/native_lb.cpp. The other
# two arms are transcriptions inside their fixtures; the tool mains cannot be
# linked without a model and are cited line by line.
#
# No absolute scratch paths. Overrides: FG_NATIVE, FG_GOOSE, CC, CXX.
set -euo pipefail

EXP="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
NATIVE=${FG_NATIVE:-/home/end0/AI/FrankenMoE-CUDA/upstream/llama.cpp}
GOOSE=${FG_GOOSE:-${GOOSE:-/home/end0/AI/LlamaServer/toolchain/goose-2026.3-string/goose}}
CCB=${CC:-gcc-13}
CXXB=${CXX:-g++-13}
OUT=$EXP/build

SLICE=${1:-all}
MODE=${2:-final}
case "$MODE" in
    final) OPT=-O1 ;;
    mutate) OPT=-O0 ;;
    *) echo "usage: build.sh [lb|tz|fp|all] [final|mutate]" >&2; exit 2 ;;
esac

mkdir -p "$OUT"

ms() { echo $(( ($1 - $2) / 1000000 )); }
VER=$("$CCB" -dumpfullversion 2>/dev/null || echo unknown)

gen_c() {
    local name=$1 src=$2
    local key
    key=$(sha256sum "$src" | awk '{print $1}')"-$OPT-$VER"
    if [ "$MODE" = mutate ] || [ ! -f "$OUT/gen-$name.o" ] || [ "$(cat "$OUT/.key-$name" 2>/dev/null || true)" != "$key" ]; then
        timeout 300 "$GOOSE" --standalone -o "$OUT/$name.c" --header "$OUT/$name.h" "$src"
        "$CCB" "$OPT" -std=c11 -w -c "$OUT/$name.c" -o "$OUT/gen-$name.o"
        printf '%s\n' "$key" > "$OUT/.key-$name"
    fi
}

native_lb() {
    local o=$OUT/native_lb.o
    if [ ! -f "$o" ] || [ "$EXP/tests/native_lb.cpp" -nt "$o" ] || [ "$NATIVE/tools/llama-bench/llama-bench.cpp" -nt "$o" ]; then
        "$CXXB" -O2 -std=c++20 -ffunction-sections -fdata-sections \
            -I"$NATIVE" -I"$NATIVE/common" -I"$NATIVE/include" -I"$NATIVE/ggml/include" \
            -c "$EXP/tests/native_lb.cpp" -o "$o"
    fi
    echo "$o"
}

run_lb() {
    local t0 t1 t2
    t0=$(date +%s%N)
    gen_c lb "$EXP/source/llama-bench-report.goose"
    local nat
    nat=$(native_lb)
    t1=$(date +%s%N)
    "$CXXB" -O2 -std=c++20 -I"$OUT" "$EXP/tests/lb_fixture.cpp" "$OUT/gen-lb.o" "$nat" \
        -Wl,--gc-sections -o "$OUT/gate-lb"
    t2=$(date +%s%N)
    "$OUT/gate-lb"
    echo "build slice=lb mode=$MODE opt=$OPT gen_ms=$(ms "$t1" "$t0") link_ms=$(ms "$t2" "$t1")"
}

run_tz() {
    local t0 t1
    t0=$(date +%s%N)
    gen_c tz "$EXP/source/tokenize-cli.goose"
    t1=$(date +%s%N)
    "$CXXB" -O2 -std=c++20 -I"$OUT" "$EXP/tests/tz_fixture.cpp" "$OUT/gen-tz.o" -o "$OUT/gate-tz"
    "$OUT/gate-tz"
    echo "build slice=tz mode=$MODE opt=$OPT gen_ms=$(ms "$t1" "$t0")"
}

run_fp() {
    local t0 t1
    t0=$(date +%s%N)
    gen_c fp "$EXP/source/fit-params-line.goose"
    t1=$(date +%s%N)
    "$CXXB" -O2 -std=c++20 -I"$OUT" "$EXP/tests/fp_fixture.cpp" "$OUT/gen-fp.o" -o "$OUT/gate-fp"
    "$OUT/gate-fp"
    echo "build slice=fp mode=$MODE opt=$OPT gen_ms=$(ms "$t1" "$t0")"
}

case "$SLICE" in
    lb) run_lb ;;
    tz) run_tz ;;
    fp) run_fp ;;
    all) run_lb; run_tz; run_fp ;;
    *) echo "usage: build.sh [lb|tz|fp|all] [final|mutate]" >&2; exit 2 ;;
esac
