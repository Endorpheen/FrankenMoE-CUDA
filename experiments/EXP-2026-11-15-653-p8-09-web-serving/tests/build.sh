#!/usr/bin/env bash
# P8-09 gate build: three web-serving slices, one script.
#
#   bash tests/build.sh              # all three gates at -O1
#   bash tests/build.sh web          # one gate
#   bash tests/build.sh mime mutate  # one gate at -O0, for a mutation run
#
# Layout, derived from this script's location:
#   tests/make_assets.py + the base-commit embed.cpp  -> build/assets, build/gen
#   source/web-assets.goose  -> build/web.c   -> gate-web   (route/serve decision)
#   source/mime-table.goose  -> build/mime.c  -> gate-mime  (type table, labels)
#   source/asset-etag.goose  -> build/et.c    -> gate-etag  (FNV-1a, ETag text)
#
# The native arm is the base-commit repository itself: tools/ui/embed.cpp is
# built as the generator that produces build/gen/ui.cpp, and the same file is
# included by two fixtures so its static helpers are callable. No model, no GPU,
# no server, no network, no shared tree is modified.
#
# No absolute scratch paths inside the experiment. Overrides: FG_REPO, FG_GOOSE,
# CC, CXX.
set -euo pipefail

EXP="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
REPO=${FG_REPO:-/home/end0/AI/FrankenMoE-CUDA}
GOOSE=${FG_GOOSE:-/home/end0/AI/LlamaServer/toolchain/goose-2026.3-string/goose}
CCB=${CC:-gcc-13}
CXXB=${CXX:-g++-13}
OUT=$EXP/build
UI_SRC=$REPO/upstream/llama.cpp/tools/ui

SLICE=${1:-all}
MODE=${2:-final}
case "$MODE" in
    final)  OPT=-O1 ;;
    mutate) OPT=-O0 ;;
    *) echo "usage: build.sh [web|mime|etag|all] [final|mutate]" >&2; exit 2 ;;
esac

mkdir -p "$OUT"
ms() { echo $(( ($1 - $2) / 1000000 )); }
VER=$("$CCB" -dumpfullversion 2>/dev/null || echo unknown)

# The synthetic bundle and the real generator output. Rebuilt only when the
# generator or the asset list is newer than the produced ui.cpp.
oracle_assets() {
    local gen=$OUT/gen/ui.cpp
    if [ ! -f "$gen" ] || [ "$EXP/tests/make_assets.py" -nt "$gen" ] || [ "$UI_SRC/embed.cpp" -nt "$gen" ]; then
        rm -rf "$OUT/assets" "$OUT/gen"
        mkdir -p "$OUT/gen"
        python3 "$EXP/tests/make_assets.py" "$OUT/assets" >/dev/null
        "$CXXB" -O2 -std=c++17 -o "$OUT/embed-native" "$UI_SRC/embed.cpp"
        "$OUT/embed-native" "$OUT/gen/ui.cpp" "$OUT/gen/ui.h" "$OUT/assets" >/dev/null
    fi
    if [ ! -f "$OUT/ui.o" ] || [ "$gen" -nt "$OUT/ui.o" ]; then
        "$CXXB" -O2 -std=c++17 -c "$gen" -o "$OUT/ui.o"
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
    "$CXXB" -O2 -std=c++17 -I"$OUT" -I"$OUT/gen" -I"$UI_SRC" \
        "$EXP/tests/$fix" "$OUT/gen-$name.o" "$OUT/ui.o" -o "$OUT/gate-$name"
}

run_gate() {
    local name=$1
    local t0 t1
    t0=$(date +%s%N)
    gen_c "$name" "$EXP/source/$2"
    build_gate "$name" "$3"
    t1=$(date +%s%N)
    set +e
    timeout 300 "$OUT/gate-$name"
    RC=$?
    set -e
    echo "gate $name mode=$MODE opt=$OPT build_ms=$(ms "$t1" "$t0")"
    return $RC
}

oracle_assets

RC=0
case "$SLICE" in
    web)  run_gate web  web-assets.goose  fixture_web.cpp  || RC=1 ;;
    mime) run_gate mime mime-table.goose  fixture_mime.cpp || RC=1 ;;
    etag) run_gate etag asset-etag.goose  fixture_etag.cpp || RC=1 ;;
    all)
        run_gate web  web-assets.goose  fixture_web.cpp  || RC=1
        run_gate mime mime-table.goose  fixture_mime.cpp || RC=1
        run_gate etag asset-etag.goose  fixture_etag.cpp || RC=1
        ;;
esac
exit $RC
