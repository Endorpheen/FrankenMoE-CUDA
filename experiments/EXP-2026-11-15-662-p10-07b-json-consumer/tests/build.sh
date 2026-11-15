#!/usr/bin/env bash
# P10-07 differential gate build. One consumer, two carriers, one corpus.
#
#   bash tests/build.sh off            # nlohmann path only, -O1
#   bash tests/build.sh on             # Goose path + nlohmann oracle, -O1
#   bash tests/build.sh all            # both, then the transcript diff and nm checks
#   bash tests/build.sh on mutate      # -O0, for a mutation run
#
# Layout, derived from this script's location:
#   source/json-parser.goose -> source/json-parser-gen.c -> build-<mode>/json-parser-gen.o
#   source/{goose-json-adapter,hf-pick}.cpp + tests/fixture.cpp -> gate-<mode>
#
# Overrides: GOOSE, CC, CXX, FG_LLAMA (holds vendor/nlohmann/json.hpp), FG_OUT.
set -euo pipefail

EXP="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
REPO=${FG_REPO:-/home/end0/AI/FrankenMoE-CUDA}
GOOSE=${GOOSE:-/home/end0/AI/LlamaServer/toolchain/goose-2026.3-string/goose}
CCB=${CC:-gcc-13}
CXXB=${CXX:-g++-13}
LLAMA=${FG_LLAMA:-$REPO/upstream/llama.cpp}
OUTROOT=${FG_OUT:-$EXP}

MODE=${1:-all}
RUN=${2:-final}
case "$RUN" in
    final)  OPT=-O1 ;;
    mutate) OPT=-O0 ;;
    *) echo "usage: build.sh [off|on|all] [final|mutate]" >&2; exit 2 ;;
esac

mkdir -p "$EXP/source"
GEN_C=$EXP/source/json-parser-gen.c
GEN_H=$EXP/source/json-parser-gen.h

# --- the one Goose module ---------------------------------------------------
gen_goose() {
    local out=$1 key cur
    mkdir -p "$out"
    key=$(sha256sum "$EXP/source/json-parser.goose" | cut -d' ' -f1)
    key="$key-$OPT-$("$GOOSE" --help 2>/dev/null | head -1 | tr ' ' '_')"
    cur=$(cat "$out/.gen-key" 2>/dev/null || true)
    if [ "$RUN" = mutate ] || [ ! -f "$out/json-parser-gen.o" ] || [ "$cur" != "$key" ]; then
        "$GOOSE" --standalone -o "$GEN_C" --header "$GEN_H" "$EXP/source/json-parser.goose" >/dev/null 2>&1
        "$CCB" "$OPT" -std=c11 -c "$GEN_C" -o "$out/json-parser-gen.o"
        printf '%s' "$key" > "$out/.gen-key"
    fi
}

# --- one carrier configuration ---------------------------------------------
build_mode() {
    local mode=$1
    local out=$EXP/build-$mode
    local flag=()
    local gen=""
    mkdir -p "$out"
    if [ "$mode" = on ]; then
        flag=(-DFG_GOOSE_JSON_REPLACE=1)
        gen=1
        gen_goose "$out"
    fi

    "$CXXB" "$OPT" -std=c++17 -Wall -Wextra -Wno-unused-parameter \
        -I"$EXP/source" -I"$LLAMA/vendor" "${flag[@]}" \
        -c "$EXP/source/hf-pick.cpp" -o "$out/hf-pick.o"
    if [ "$mode" = on ]; then
        "$CXXB" "$OPT" -std=c++17 -Wall -Wextra -Wno-unused-parameter \
            -I"$EXP/source" "${flag[@]}" \
            -c "$EXP/source/goose-json-adapter.cpp" -o "$out/goose-json-adapter.o"
    fi
    "$CXXB" "$OPT" -std=c++17 -Wall -Wextra -Wno-unused-parameter \
        -I"$EXP/source" -I"$LLAMA/vendor" "${flag[@]}" \
        -c "$EXP/tests/fixture.cpp" -o "$out/fixture.o"

    local objs=("$out/hf-pick.o" "$out/fixture.o")
    [ -n "$gen" ] && objs+=("$out/goose-json-adapter.o" "$out/json-parser-gen.o")
    "$CXXB" "$OPT" "${objs[@]}" -o "$out/gate"
}

case "$MODE" in
    off) build_mode off ;;
    on)  build_mode on ;;
    all) build_mode off; build_mode on ;;
    *) echo "usage: build.sh [off|on|all] [final|mutate]" >&2; exit 2 ;;
esac

if [ "$MODE" = all ]; then
    mkdir -p "$EXP/evidence"
    set +e
    "$EXP/build-off/gate" --cases "$EXP/tests/cases" > "$EXP/evidence/gate-off.log" 2>&1
    RC_OFF=$?
    "$EXP/build-on/gate"  --cases "$EXP/tests/cases" > "$EXP/evidence/gate-on.log" 2>&1
    RC_ON=$?
    set -e
    echo "run off rc=$RC_OFF: $(tail -1 "$EXP/evidence/gate-off.log")"
    echo "run on  rc=$RC_ON:  $(tail -1 "$EXP/evidence/gate-on.log")"

    "$EXP/build-off/gate" --dump --cases "$EXP/tests/cases" > "$EXP/evidence/dump-off.txt" 2>&1
    "$EXP/build-on/gate"  --dump --cases "$EXP/tests/cases" > "$EXP/evidence/dump-on.txt" 2>&1
    if diff -q "$EXP/evidence/dump-off.txt" "$EXP/evidence/dump-on.txt" >/dev/null; then
        echo "switch off/on transcript identical: yes ($(wc -l < "$EXP/evidence/dump-on.txt") lines)"
    else
        echo "switch off/on transcript identical: NO"
        diff "$EXP/evidence/dump-off.txt" "$EXP/evidence/dump-on.txt" || true
    fi

    # Symbols: one goose_init and one json_parse in ON, none in OFF; both pick
    # bodies present in ON, only the nlohmann one in OFF.
    for m in off on; do
        b=$EXP/build-$m/gate
        printf '%s goose_init=%s json_parse=%s hf_pick_goose=%s pick_goose=%s pick_nlohmann=%s hf_pick_native=%s\n' "$m" \
            "$(nm -C --defined-only "$b" | grep -cw goose_init || true)" \
            "$(nm -C --defined-only "$b" | grep -cw json_parse || true)" \
            "$(nm -C --defined-only "$b" | grep -c 'T hf_pick_goose' || true)" \
            "$(nm -C --defined-only "$b" | grep -c 'pick_with<goose_json::' || true)" \
            "$(nm -C --defined-only "$b" | grep -c 'pick_with<nlohmann::' || true)" \
            "$(nm -C --defined-only "$b" | grep -c 'T hf_pick_native' || true)"
    done | tee "$EXP/evidence/nm-symbols.txt"

    if [ "$RC_OFF" -ne 0 ] || [ "$RC_ON" -ne 0 ]; then
        exit 1
    fi
fi
