#!/usr/bin/env bash
# P10-08 gate build: device headers, ggml-cpp.h + carrier headers, scheduler
# eval sequence.
#
#   bash tests/build.sh                 # all three gates at -O1
#   bash tests/build.sh devh            # one gate
#   bash tests/build.sh sched mutate    # one gate, module compiled at -O0
#
# Layout, derived from this script's location:
#   source/device-headers.goose -> build/devh.c  -> gate-devh
#   source/cpp-headers.goose    -> build/cpph.c  -> gate-cpph
#   source/sched-eval.goose     -> build/sched.c -> gate-sched
#
# Before compiling, tests/extract_native.py re-reads the fourteen device
# headers and the two callback source sites. The fixtures then expand every
# macro and classify every declaration with decltype, so no gate value is
# compared against a hand copy.
#
# No absolute scratch paths. Overrides: FG_NATIVE, FG_GOOSE, FG_CARRIER, CC,
# CXX, FG_OUT, FG_SO_DIR.
set -euo pipefail

EXP="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
NATIVE=${FG_NATIVE:-/home/end0/AI/FrankenMoE-CUDA/upstream/llama.cpp}
CARRIER=${FG_CARRIER:-/home/end0/AI/FrankenMoE-CUDA/runtime/core/include}
SO=${FG_SO_DIR:-/home/end0/AI/FrankenMoE-CUDA/build/exp054-dflash/bin}
GOOSE=${FG_GOOSE:-${GOOSE:-/home/end0/AI/LlamaServer/toolchain/goose-2026.3-string/goose}}
CCB=${CC:-gcc-13}
CXXB=${CXX:-g++-13}
OUT=${FG_OUT:-$EXP/build}

SLICE=${1:-all}
MODE=${2:-final}
case "$MODE" in
    final) OPT=-O1 ;;
    mutate) OPT=-O0 ;;
    *) echo "usage: build.sh [devh|cpph|sched|all] [final|mutate]" >&2; exit 2 ;;
esac

mkdir -p "$OUT" "$OUT/native"

ms() { echo $(( ($1 - $2) / 1000000 )); }
VER=$("$CCB" -dumpfullversion 2>/dev/null || echo unknown)

INC=(-I"$NATIVE/include" -I"$NATIVE/ggml/include" -I"$NATIVE/ggml/src" -I"$CARRIER" -I"$OUT/native" -I"$OUT")

# ── the oracle, rebuilt on every invocation ──────────────────────────────────
T0=$(date +%s%N)
python3 "$EXP/tests/extract_native.py" "$NATIVE" "$OUT/native" | sed 's/^/  /'
T1=$(date +%s%N)

gen_c() {
    local name=$1 src=$2
    local key
    key=$(sha256sum "$src" | awk '{print $1}')"-$OPT-$VER"
    if [ "$MODE" = mutate ] || [ ! -f "$OUT/gen-$name.o" ] || [ "$(cat "$OUT/.key-$name" 2>/dev/null || true)" != "$key" ]; then
        timeout 300 "$GOOSE" --standalone -o "$OUT/$name.c" --header "$OUT/${name}_gen.h" "$src" >/dev/null
        "$CCB" "$OPT" -std=c11 -w -c "$OUT/$name.c" -o "$OUT/gen-$name.o"
        printf '%s\n' "$key" > "$OUT/.key-$name"
    fi
}

build() {
    local name=$1 src=$2 fix=$3 sum=$4
    local t0 t1
    t0=$(date +%s%N)
    gen_c "$name" "$src"
    t1=$(date +%s%N)
    "$CXXB" -O2 -std=c++17 "${INC[@]}" "$EXP/tests/$fix" "$OUT/gen-$name.o" \
        -o "$OUT/gate-$name"
    "$OUT/gate-$name" | tee "$OUT/gate-$name.log"
    grep -q "$sum" "$OUT/gate-$name.log" || { echo "FAIL $name: no summary" >&2; exit 1; }
    grep -q "failures=0" "$OUT/gate-$name.log" || { echo "FAIL $name: non-zero failures" >&2; exit 1; }
    echo "build slice=$name mode=$MODE opt=$OPT gen_ms=$(ms "$t1" "$t0")"
}

run_devh() { build devh "$EXP/source/device-headers.goose" fixture_devh.cpp FG659_DEVH; }

run_cpph() {
    local t0 t1
    t0=$(date +%s%N)
    gen_c cpph "$EXP/source/cpp-headers.goose"
    t1=$(date +%s%N)
    "$CXXB" -O2 -std=c++17 -I"$CARRIER" -c "$EXP/tests/version_probe.cpp" \
        -DVERSION_FN=probe_version_defined '-DBMOE_VERSION="9.9.9-test"' -o "$OUT/ver-defined.o"
    "$CXXB" -O2 -std=c++17 -I"$CARRIER" -c "$EXP/tests/version_probe.cpp" \
        -DVERSION_FN=probe_version_plain -o "$OUT/ver-plain.o"
    "$CXXB" -O2 -std=c++17 "${INC[@]}" "$EXP/tests/fixture_cpph.cpp" \
        "$OUT/ver-defined.o" "$OUT/ver-plain.o" "$OUT/gen-cpph.o" -o "$OUT/gate-cpph" \
        -Wl,--wrap=ggml_free -Wl,--wrap=gguf_free -Wl,--wrap=ggml_gallocr_free \
        -Wl,--wrap=ggml_backend_free -Wl,--wrap=ggml_backend_buffer_free \
        -Wl,--wrap=ggml_backend_event_free -Wl,--wrap=ggml_backend_sched_free
    "$OUT/gate-cpph" | tee "$OUT/gate-cpph.log"
    grep -q FG659_CPPH "$OUT/gate-cpph.log" || { echo "FAIL cpph: no summary" >&2; exit 1; }
    grep -q "failures=0" "$OUT/gate-cpph.log" || { echo "FAIL cpph: non-zero failures" >&2; exit 1; }
    echo "build slice=cpph mode=$MODE opt=$OPT gen_ms=$(ms "$t1" "$t0")"
}

run_sched() {
    local t0 t1
    t0=$(date +%s%N)
    gen_c sched "$EXP/source/sched-eval.goose"
    t1=$(date +%s%N)
    "$CXXB" -O2 -std=c++17 "${INC[@]}" "$EXP/tests/fixture_sched.cpp" \
        "$OUT/gen-sched.o" -L"$SO" -lggml-base -Wl,-rpath,"$SO" -o "$OUT/gate-sched"
    "$OUT/gate-sched" | tee "$OUT/gate-sched.log"
    grep -q FG659_SCHED "$OUT/gate-sched.log" || { echo "FAIL sched: no summary" >&2; exit 1; }
    grep -q "failures=0" "$OUT/gate-sched.log" || { echo "FAIL sched: non-zero failures" >&2; exit 1; }
    echo "build slice=sched mode=$MODE opt=$OPT gen_ms=$(ms "$t1" "$t0")"
}

case "$SLICE" in
    devh) run_devh ;;
    cpph) run_cpph ;;
    sched) run_sched ;;
    all) run_devh; run_cpph; run_sched ;;
    *) echo "usage: build.sh [devh|cpph|sched|all] [final|mutate]" >&2; exit 2 ;;
esac
echo "oracle_ms=$(ms "$T1" "$T0")"
