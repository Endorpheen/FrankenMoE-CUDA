#!/bin/bash
# S1.4 mutation check: proves the gate observes every decision it claims.
#
# For each mutation: rewrite one decision of the staged module, recompose, regenerate,
# relink the ON library, rerun the same gate. "caught" means the gate reported failures.
# The module and the generated C are restored to the unmutated state on exit.
#
# Usage: scripts/metrics-mutation.sh [1|2|3|4|all]
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
GOOSE=${GOOSE:-/home/end0/AI/LlamaServer/toolchain/goose-2026.3-string/goose}
COMPOSER=${COMPOSER:-${CARRIER:-/home/end0/AI/FrankenMoE-CUDA}/tools/compose-goose.py}
J=${J:-32}
WHICH=${1:-all}

cd "$ROOT"
SRC=goose/metrics/metrics-traces.goose
cp "$SRC" goose-gen/.metrics-traces.orig

rebuild() {
    python3 "$COMPOSER" goose-gen/metrics-traces.composed.goose "$SRC" >/dev/null
    "$GOOSE" --standalone -o goose-gen/metrics-traces.c --header goose-gen/metrics-traces.h \
             goose-gen/metrics-traces.composed.goose >/dev/null 2>&1
    cmake --build build-metrics-on -j "$J" --target fg-goose-metrics-smoke >/dev/null 2>&1
}

restore() {
    cp goose-gen/.metrics-traces.orig "$SRC"
    rm -f goose-gen/.metrics-traces.orig
    rebuild
}
trap restore EXIT

run_one() {
    local id=$1
    cp goose-gen/.metrics-traces.orig "$SRC"
    python3 scripts/metrics-mutate.py "$id"
    rebuild
    local dir
    dir=$(mktemp -d)
    set +e
    ./build-metrics-on/fg-goose-metrics-smoke "$dir" > "$dir.out" 2>&1
    set -e
    local fails
    fails=$(sed -n 's/.*failures=\([0-9]*\).*/\1/p' "$dir.out" | tail -1)
    if [ "${fails:-0}" -gt 0 ]; then
        printf 'MUT %s caught   failures=%s\n' "$id" "$fails"
    else
        printf 'MUT %s SURVIVED failures=0\n' "$id"
    fi
    rm -rf "$dir" "$dir.out"
}

for id in 1 2 3 4; do
    if [ "$WHICH" = all ] || [ "$WHICH" = "$id" ]; then
        run_one "$id"
    fi
done
