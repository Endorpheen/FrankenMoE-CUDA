#!/usr/bin/env bash
# P8-09 batch 2 gate build: three web-serving slices, one script.
#
#   bash tests/build.sh              # all three gates at -O1
#   bash tests/build.sh cors         # one gate
#   bash tests/build.sh qry mutate   # one gate at -O0, for a mutation run
#
# Layout, derived from this script's location:
#   source/cors-policy.goose      -> build/cors.c -> gate-cors  (CORS decision)
#   source/query-transport.goose  -> build/qry.c  -> gate-qry   (decode, query)
#   source/api-key-policy.goose   -> build/key.c  -> gate-key   (API-key middleware)
#
# The native arm is the base-commit repository itself: the fixtures include
# common/http.h and the vendored cpp-httplib, and gate-qry links the real
# httplib.cpp so the query transport is compared against the vendored code, not
# against a re-reading of it. No model, no GPU, no server, no network, and no
# file of the shared tree is modified. Scratch space stays under this experiment
# directory.
#
# Overrides: FG_REPO, FG_GOOSE, CC, CXX.
set -euo pipefail

EXP="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
REPO=${FG_REPO:-/home/end0/AI/FrankenMoE-CUDA}
GOOSE=${FG_GOOSE:-/home/end0/AI/LlamaServer/toolchain/goose-2026.3-string/goose}
CCB=${CC:-gcc-13}
CXXB=${CXX:-g++-13}
OUT=$EXP/build
VENDOR=$REPO/upstream/llama.cpp/vendor
COMMON=$REPO/upstream/llama.cpp/common

SLICE=${1:-all}
MODE=${2:-final}
case "$MODE" in
    final)  OPT=-O1 ;;
    mutate) OPT=-O0 ;;
    *) echo "usage: build.sh [cors|qry|key|all] [final|mutate]" >&2; exit 2 ;;
esac

mkdir -p "$OUT"
ms() { echo $(( ($1 - $2) / 1000000 )); }
VER=$("$CCB" -dumpfullversion 2>/dev/null || echo unknown)

# The vendored httplib translation unit, needed only by the query gate. Built
# once at -O1: mutations only ever change the Goose module under test.
httplib_obj() {
    local obj=$OUT/httplib.o
    if [ ! -f "$obj" ] || [ "$VENDOR/cpp-httplib/httplib.cpp" -nt "$obj" ]; then
        "$CXXB" -O1 -std=c++17 -pthread -I"$VENDOR" -c "$VENDOR/cpp-httplib/httplib.cpp" -o "$obj"
    fi
}

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

build_gate() {
    local name=$1 fix=$2
    local extra=()
    if [ "$name" = qry ]; then
        httplib_obj
        extra=("$OUT/httplib.o" -pthread)
    fi
    "$CXXB" -O2 -std=c++17 -pthread -I"$OUT" -I"$COMMON" -I"$VENDOR" \
        "$EXP/tests/$fix" "$OUT/gen-$name.o" "${extra[@]}" -o "$OUT/gate-$name"
}

run_gate() {
    local name=$1
    local t0 t1
    t0=$(date +%s%N)
    gen_c "$name" "$EXP/source/$3"
    build_gate "$name" "$2"
    t1=$(date +%s%N)
    set +e
    timeout 300 "$OUT/gate-$name"
    RC=$?
    set -e
    echo "gate $name mode=$MODE opt=$OPT build_ms=$(ms "$t1" "$t0")"
    return $RC
}

RC=0
case "$SLICE" in
    cors) run_gate cors fixture_cors.cpp cors-policy.goose      || RC=1 ;;
    qry)  run_gate qry  fixture_qry.cpp  query-transport.goose  || RC=1 ;;
    key)  run_gate key  fixture_key.cpp  api-key-policy.goose   || RC=1 ;;
    all)
        run_gate cors fixture_cors.cpp cors-policy.goose      || RC=1
        run_gate qry  fixture_qry.cpp  query-transport.goose  || RC=1
        run_gate key  fixture_key.cpp  api-key-policy.goose   || RC=1
        ;;
esac
exit $RC
