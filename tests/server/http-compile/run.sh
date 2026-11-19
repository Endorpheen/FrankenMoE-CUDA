#!/usr/bin/env bash
# Permanent check: the shipped server's HTTP source still compiles.
#
# server-http.cpp depends on a *generated* ui.h, so it can only be compiled with a configured tree's
# include path - which is why no CI job covers it: the C++ job compiles models/*.cpp and the Goose
# job compiles the unit. A change there was therefore unverified until this check existed.
#
# Tier: fast - one translation unit, syntax only, a few seconds.
#
# Usage: bash tests/server/http-compile/run.sh [configured-tree]
set -uo pipefail
D="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$D/../../.." && pwd)"
OUT=$D/build
LOG=$OUT/run.log
mkdir -p "$OUT"
TREE=${1:-$ROOT/builds/flint-acc}

if [ ! -f "$TREE/tools/ui/ui.h" ]; then
    printf 'skip: %s has no generated tools/ui/ui.h, so the source cannot be compiled\n' "${TREE#$ROOT/}" | tee "$LOG"
    printf '      configure a tree first (cmake -S vendor/flint -B <tree>)\n' | tee -a "$LOG"
    exit 77
fi

{
    printf 'tree:   %s\n' "${TREE#$ROOT/}"
    printf 'source: %s\n' "$(git -C "$ROOT" rev-parse --short HEAD 2>/dev/null || echo unknown)"
    rc=0
    for src in server-http.cpp server-tools.cpp; do
        printf '$ g++-13 -fsyntax-only vendor/flint/tools/server/%s\n' "$src"
        if g++-13 -std=c++17 -fsyntax-only \
            -I"$ROOT/vendor/flint/tools/server" -I"$ROOT/vendor/flint/common" \
            -I"$ROOT/vendor/flint/include" -I"$ROOT/vendor/flint/ggml/include" \
            -I"$ROOT/vendor/flint/vendor" -I"$ROOT/vendor/flint/tools/mtmd" \
            -I"$TREE/tools/ui" -I"$ROOT/vendor/flint/src" \
            "$ROOT/vendor/flint/tools/server/$src"; then
            printf 'ok: %s compiles\n' "$src"
        else
            printf 'FAIL: %s does not compile\n' "$src"
            rc=1
        fi
    done
    exit "$rc"
} 2>&1 | tee "$LOG"
rc=${PIPESTATUS[0]}
printf '[exit %d]\n' "$rc" | tee -a "$LOG"
exit "$rc"
