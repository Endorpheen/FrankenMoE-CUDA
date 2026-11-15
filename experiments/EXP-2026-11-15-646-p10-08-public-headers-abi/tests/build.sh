#!/usr/bin/env bash
# P10-08 gate build: three public-ABI slices, one script.
#
#   bash tests/build.sh                  # all three gates at -O1
#   bash tests/build.sh abi              # one gate
#   bash tests/build.sh nm mutate        # one gate at -O0, for a mutation run
#
# Layout, derived from this script's location:
#   source/ggml-type-table.goose     -> build/abi.c -> gate-abi
#   source/public-enum-values.goose  -> build/hdr.c -> gate-hdr
#   source/public-name-tables.goose  -> build/nm.c  -> gate-nm
#
# Before compiling, tests/extract_native.py parses the native tables out of
# $FG_NATIVE and writes build/native/. That is the oracle: nothing in the gates
# is compared against a hand-copied native table.
#
# No absolute scratch paths. Overrides: FG_NATIVE, FG_GOOSE, CC, CXX, FG_OUT.
set -euo pipefail

EXP="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
NATIVE=${FG_NATIVE:-/home/end0/AI/FrankenMoE-CUDA/upstream/llama.cpp}
GOOSE=${FG_GOOSE:-${GOOSE:-/home/end0/AI/LlamaServer/toolchain/goose-2026.3-string/goose}}
CCB=${CC:-gcc-13}
CXXB=${CXX:-g++-13}
OUT=${FG_OUT:-$EXP/build}

SLICE=${1:-all}
MODE=${2:-final}
case "$MODE" in
    final) OPT=-O1 ;;
    mutate) OPT=-O0 ;;
    *) echo "usage: build.sh [abi|hdr|nm|all] [final|mutate]" >&2; exit 2 ;;
esac

mkdir -p "$OUT" "$OUT/native"

ms() { echo $(( ($1 - $2) / 1000000 )); }
VER=$("$CCB" -dumpfullversion 2>/dev/null || echo unknown)

INC=(-I"$NATIVE/include" -I"$NATIVE/ggml/include" -I"$NATIVE/ggml/src" -I"$OUT/native")

# ── the oracle, rebuilt from the native sources on every invocation ──────────
T0=$(date +%s%N)
python3 "$EXP/tests/extract_native.py" "$NATIVE" "$OUT/native" | sed 's/^/  /'
T1=$(date +%s%N)

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

build() {
    local name=$1 src=$2 fix=$3 sum=$4
    local t0 t1
    t0=$(date +%s%N)
    gen_c "$name" "$src"
    t1=$(date +%s%N)
    "$CXXB" -O2 -std=c++17 "${INC[@]}" -I"$OUT" "$EXP/tests/$fix" "$OUT/gen-$name.o" \
        -o "$OUT/gate-$name"
    "$OUT/gate-$name" | tee "$OUT/gate-$name.log"
    grep -q "failures=0" "$OUT/gate-$name.log" || { echo "FAIL $name: non-zero failures" >&2; exit 1; }
    echo "build slice=$name mode=$MODE opt=$OPT gen_ms=$(ms "$t1" "$t0")"
}

run_abi() { build abi "$EXP/source/ggml-type-table.goose"    fixture_abi.cpp FG645_ABI; }
run_hdr() { build hdr "$EXP/source/public-enum-values.goose" fixture_hdr.cpp FG645_HDR; }
run_nm()  { build nm  "$EXP/source/public-name-tables.goose" fixture_nm.cpp  FG645_NM; }

case "$SLICE" in
    abi) run_abi ;;
    hdr) run_hdr ;;
    nm)  run_nm ;;
    all) run_abi; run_hdr; run_nm ;;
    *) echo "usage: build.sh [abi|hdr|nm|all] [final|mutate]" >&2; exit 2 ;;
esac
echo "oracle_ms=$(ms "$T1" "$T0")"
