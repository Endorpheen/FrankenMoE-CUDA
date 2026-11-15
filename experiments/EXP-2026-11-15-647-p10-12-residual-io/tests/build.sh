#!/usr/bin/env bash
# P10-12 gate build: three residual-slice gates, one script.
#
#   bash tests/build.sh              # all three gates at -O1
#   bash tests/build.sh fr           # one gate
#   bash tests/build.sh sn mutate    # one gate at -O0, for a mutation run
#
# Layout, derived from this script's location:
#   source/file_reader_windows.goose -> build/fr.c -> gate-fr (native reader + pio)
#   source/proc-readers.goose        -> build/pr.c -> gate-pr (pio included)
#   source/shard-naming.goose        -> build/sn.c -> gate-sn (gguf_offsets included)
#
# The native arm is the base-commit repository itself: file_reader.cpp and
# platform_io.cpp are compiled, platform_io.cpp and gguf_offsets.cpp are also
# included by their fixtures where an anonymous-namespace function is needed.
# No model, no GPU, no llama library, no shared tree is modified.
#
# No absolute scratch paths inside the experiment. Overrides: FG_REPO,
# FG_NATIVE_INC (ggml headers), FG_GOOSE, CC, CXX.
set -euo pipefail

EXP="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
REPO=${FG_REPO:-/home/end0/AI/FrankenMoE-CUDA}
GINC=${FG_NATIVE_INC:-/home/end0/AI/LlamaServer/llama.cpp-flint-cuda/ggml/include}
GOOSE=${FG_GOOSE:-${GOOSE:-/home/end0/AI/LlamaServer/toolchain/goose-2026.3-string/goose}}
CCB=${CC:-gcc-13}
CXXB=${CXX:-g++-13}
OUT=$EXP/build

SLICE=${1:-all}
MODE=${2:-final}
case "$MODE" in
    final) OPT=-O1 ;;
    mutate) OPT=-O0 ;;
    *) echo "usage: build.sh [fr|pr|sn|all] [final|mutate]" >&2; exit 2 ;;
esac

mkdir -p "$OUT"

ms() { echo $(( ($1 - $2) / 1000000 )); }
VER=$("$CCB" -dumpfullversion 2>/dev/null || echo unknown)

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

# The two native translation units the fr gate links.
native_io() {
    local o=$OUT/native-$1.o
    local src=$REPO/runtime/core/src/io/$1.cpp
    if [ ! -f "$o" ] || [ "$src" -nt "$o" ]; then
        "$CXXB" -O2 -std=c++17 -I"$REPO/runtime/core/src/io" -c "$src" -o "$o"
    fi
    echo "$o"
}

run_fr() {
    local t0 t1 t2
    t0=$(date +%s%N)
    gen_c fr "$EXP/source/file_reader_windows.goose"
    local nf np
    nf=$(native_io file_reader)
    np=$(native_io platform_io)
    t1=$(date +%s%N)
    "$CXXB" -O2 -std=c++17 -I"$OUT" -I"$REPO/runtime/core/src/io" \
        "$EXP/tests/fr_fixture.cpp" "$OUT/gen-fr.o" "$nf" "$np" -o "$OUT/gate-fr"
    t2=$(date +%s%N)
    "$OUT/gate-fr"
    echo "build slice=fr mode=$MODE opt=$OPT gen_ms=$(ms "$t1" "$t0") link_ms=$(ms "$t2" "$t1")"
}

run_pr() {
    local t0 t1
    t0=$(date +%s%N)
    gen_c pr "$EXP/source/proc-readers.goose"
    t1=$(date +%s%N)
    "$CXXB" -O2 -std=c++17 -I"$OUT" -I"$REPO/runtime/core/src/io" \
        "$EXP/tests/pr_fixture.cpp" "$OUT/gen-pr.o" -o "$OUT/gate-pr"
    "$OUT/gate-pr"
    echo "build slice=pr mode=$MODE opt=$OPT gen_ms=$(ms "$t1" "$t0")"
}

run_sn() {
    local t0 t1
    t0=$(date +%s%N)
    gen_c sn "$EXP/source/shard-naming.goose"
    t1=$(date +%s%N)
    "$CXXB" -O2 -std=c++17 -ffunction-sections -fdata-sections -Wl,--gc-sections \
        -I"$OUT" -I"$REPO/runtime/core/src/moe" -I"$GINC" \
        "$EXP/tests/sn_fixture.cpp" "$OUT/gen-sn.o" -o "$OUT/gate-sn"
    "$OUT/gate-sn"
    echo "build slice=sn mode=$MODE opt=$OPT gen_ms=$(ms "$t1" "$t0")"
}

case "$SLICE" in
    fr) run_fr ;;
    pr) run_pr ;;
    sn) run_sn ;;
    all) run_fr; run_pr; run_sn ;;
    *) echo "usage: build.sh [fr|pr|sn|all] [final|mutate]" >&2; exit 2 ;;
esac
