#!/usr/bin/env bash
# P10-08 gate build: backend headers, GGUF headers, SONAME/export surface.
#
#   bash tests/build.sh                 # all three gates at -O1
#   bash tests/build.sh bkh             # one gate
#   bash tests/build.sh ggh mutate      # one gate, module compiled at -O0
#
# Layout, derived from this script's location:
#   source/backend-headers.goose  -> build/bkh.c -> gate-bkh
#   source/gguf-headers.goose     -> build/ggh.c -> gate-ggh
#   source/soname-exports.goose   -> build/son.c -> gate-son
#
# Before compiling, tests/extract_native.py re-derives the native tables from the
# llama.cpp checkout, its CMakeLists, the reference shared objects and the ABI
# ledger. That is the oracle: no gate value is compared against a hand copy.
#
# No absolute scratch paths. Overrides: FG_NATIVE, FG_GOOSE, CC, CXX, FG_OUT,
# FG_SO_DIR, FG_LEDGER.
set -euo pipefail

EXP="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
NATIVE=${FG_NATIVE:-/home/end0/AI/FrankenMoE-CUDA/upstream/llama.cpp}
GOOSE=${FG_GOOSE:-${GOOSE:-/home/end0/AI/LlamaServer/toolchain/goose-2026.3-string/goose}}
CCB=${CC:-gcc-13}
CXXB=${CXX:-g++-13}
OUT=${FG_OUT:-$EXP/build}
REPO=${FG_REPO:-/home/end0/AI/FrankenMoE-CUDA}

SLICE=${1:-all}
MODE=${2:-final}
case "$MODE" in
    final) OPT=-O1 ;;
    mutate) OPT=-O0 ;;
    *) echo "usage: build.sh [bkh|ggh|son|all] [final|mutate]" >&2; exit 2 ;;
esac

mkdir -p "$OUT" "$OUT/native"

ms() { echo $(( ($1 - $2) / 1000000 )); }
VER=$("$CCB" -dumpfullversion 2>/dev/null || echo unknown)

INC=(-I"$NATIVE/include" -I"$NATIVE/ggml/include" -I"$NATIVE/ggml/src" -I"$OUT/native" -I"$OUT")

# ── the oracle, rebuilt on every invocation ──────────────────────────────────
T0=$(date +%s%N)
python3 "$EXP/tests/extract_native.py" "$NATIVE" "$OUT/native" "$REPO" | sed 's/^/  /'
"$CCB" -O1 -std=c11 -c "$OUT/native/native_abi.c" -o "$OUT/native/native_abi.o"
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
    "$CXXB" -O2 -std=c++17 "${INC[@]}" "$EXP/tests/$fix" "$OUT/gen-$name.o" \
        "$OUT/native/native_abi.o" -ldl -o "$OUT/gate-$name"
    "$OUT/gate-$name" | tee "$OUT/gate-$name.log"
    grep -q "$sum" "$OUT/gate-$name.log" || { echo "FAIL $name: no summary" >&2; exit 1; }
    grep -q "failures=0" "$OUT/gate-$name.log" || { echo "FAIL $name: non-zero failures" >&2; exit 1; }
    echo "build slice=$name mode=$MODE opt=$OPT gen_ms=$(ms "$t1" "$t0")"
}

run_bkh() { build bkh "$EXP/source/backend-headers.goose" fixture_bkh.cpp FG651_BKH; }
run_ggh() { build ggh "$EXP/source/gguf-headers.goose" fixture_ggh.cpp FG651_GGH; }
run_son() { build son "$EXP/source/soname-exports.goose" fixture_son.cpp FG651_SON; }

case "$SLICE" in
    bkh) run_bkh ;;
    ggh) run_ggh ;;
    son) run_son ;;
    all) run_bkh; run_ggh; run_son ;;
    *) echo "usage: build.sh [bkh|ggh|son|all] [final|mutate]" >&2; exit 2 ;;
esac
echo "oracle_ms=$(ms "$T1" "$T0")"
