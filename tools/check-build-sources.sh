#!/usr/bin/env bash
# Pre-flight for a long build: verifies that the sources and symbols named in
# docs/goose-migration/BUILD-REGISTRY.md are present, and stops with a specific
# message instead of letting a slow compile discover the problem.
#
# Usage: bash tools/check-build-sources.sh [--tree <dir>]
# Exit: 0 ok, 1 a required input is missing, 2 usage.
set -uo pipefail
ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
fail=0
say() { printf '%s\n' "$*"; }
bad() { printf 'MISSING: %s\n' "$*"; fail=1; }

TREE=${ROOT}/vendor/flint
if [ "${1:-}" = "--tree" ] && [ -n "${2:-}" ]; then
    TREE=$2
fi

say "pre-flight for tree: $TREE"

for f in CMakeLists.txt include/llama.h common/CMakeLists.txt ggml/CMakeLists.txt \
         ggml/src/CMakeLists.txt src/CMakeLists.txt src/llama-sampler.cpp; do
    [ -f "$TREE/$f" ] || bad "$TREE/$f"
done

# the tree must be self-contained: no fg* bridges in src means it links alone
if grep -rqE "fg[0-9]+[sk]?_[a-z_]+" "$TREE/src" 2>/dev/null; then
    say "note: $TREE/src calls Goose bridges; they must be linked in"
    for id in 199 210 212 230 237 244 367 373 375 376 377 380 381 382 383 384 385 386 387 388 389 390 391 392 393 394 395 396 397 398 401 402 403 404 405 406 756; do
        if ! ls "$ROOT"/integration/modules/EXP-*-"$id"-*/source/host-qualified.c >/dev/null 2>&1; then
            if grep -rq "fg${id}s_\|fg${id}k_" "$TREE/src" 2>/dev/null; then
                bad "bridge source for fg$id (integration/modules/EXP-*-$id-*)"
            fi
        fi
    done
fi

# the allocator bridge is called from ggml and must be provided somewhere
if grep -q "fg161_bulk" "$TREE/ggml/src/ggml-alloc.c" 2>/dev/null; then
    if ! grep -rqs "fg161_bulk" "$ROOT/goose/cpu/goose-allocator" "$ROOT/integration" 2>/dev/null; then
        bad "fg161_bulk is called by ggml but no source in the repository defines it"
    fi
fi

# the accepted server expects soname 0.7.0; a different one will not load it
if [ -f "$TREE/CMakeLists.txt" ]; then
    v=$(grep -m1 -oE "LLAMA_VERSION_[A-Z]+ *[0-9]+" "$TREE"/CMakeLists.txt 2>/dev/null | head -1)
    [ -n "$v" ] && say "version marker: $v"
fi

if [ "$fail" -ne 0 ]; then
    say ""
    say "See docs/goose-migration/BUILD-REGISTRY.md for the measured combinations."
    exit 1
fi
say "pre-flight ok"
