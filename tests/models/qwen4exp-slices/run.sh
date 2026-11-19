#!/usr/bin/env bash
# Permanent gate for the Qwen4Exp slices: metadata, tensor map, expert layout, and their join.
#
# Each slice is built with the modules from goose/ (not from the experiment's copies), so a change
# to the canonical module is what the gate sees. The fixtures are independent transcriptions of the
# native lines named in each module header; no native library is linked.
#
# Tier: integration - compiles four small modules, no model, no GPU. The whole run is about a
# minute; each slice on its own is a few seconds.
#
# Usage: bash tests/models/qwen4exp-slices/run.sh [q4m|q4t|q4e|join|all]
set -uo pipefail
D="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$D/../../.." && pwd)"
G=${GOOSE_BIN:-/home/end0/AI/LlamaServer/toolchain/goose-2026.3-string/goose}
OUT=$D/build
LOG=$OUT/run.log
mkdir -p "$OUT"
WHICH=${1:-all}

build_one() { # <slice> <goose-module-list> <fixture>
    local slice=$1 mods=$2 fix=$3
    local gen=$OUT/$slice
    mkdir -p "$gen"
    # shellcheck disable=SC2086
    python3 "$ROOT/tools/compose-goose.py" "$gen/gen.goose" $mods >/dev/null || return 1
    "$G" --standalone -o "$gen/gen.c" --header "$gen/gen.h" "$gen/gen.goose" >/dev/null || return 1
    gcc-13 -O1 -std=c11 -c "$gen/gen.c" -o "$gen/gen.o" || return 1
    # The abort primitive the unit shares. A fixture that already defines it keeps its own.
    if grep -q fg200g_fail "$D/$fix"; then
        : > "$gen/stub.o"
        gcc-13 -O1 -std=c11 -c -x c /dev/null -o "$gen/stub.o" || return 1
    else
        printf 'void fg200g_fail(long code){ (void) code; __builtin_trap(); }\n' > "$gen/stub.c" || return 1
        gcc-13 -O1 -std=c11 -c "$gen/stub.c" -o "$gen/stub.o" || return 1
    fi
    g++-13 -O2 -std=c++17 -I"$gen" -c "$D/$fix" -o "$gen/fixture.o" || return 1
    g++-13 "$gen/fixture.o" "$gen/gen.o" "$gen/stub.o" -o "$gen/gate" || return 1
    "$gen/gate"
}

run_slice() {
    case "$1" in
        q4m) build_one q4m  "$ROOT/goose/model/q4m-metadata.goose"                  fixture_q4m.cpp ;;
        q4t) build_one q4t  "$ROOT/goose/model/q4t-tensor-map.goose"                fixture_q4t.cpp ;;
        q4e) build_one q4e  "$ROOT/goose/model/q4e-expert-layout.goose $ROOT/goose/model/goose-model-graph.goose" fixture_q4e.cpp ;;
        join) build_one join "$ROOT/goose/model/q4t-tensor-map.goose $ROOT/goose/model/qwen4exp.goose" join_check.cpp ;;
    esac
}

if [ ! -x "$G" ]; then
    printf 'skip: no Goose compiler at %s\n' "$G" | tee "$LOG"
    exit 77
fi

{
    printf 'compiler: %s\n' "$G"
    printf 'source:   %s\n' "$(git -C "$ROOT" rev-parse --short HEAD 2>/dev/null || echo unknown)"
    rc=0
    for s in $( [ "$WHICH" = all ] && echo q4m q4t q4e join || echo "$WHICH" ); do
        printf '\n== %s\n' "$s"
        run_slice "$s" || { printf 'FAIL %s\n' "$s"; rc=1; }
    done
    printf '\n[exit %d]\n' "$rc"
    exit "$rc"
} 2>&1 | tee "$LOG"
