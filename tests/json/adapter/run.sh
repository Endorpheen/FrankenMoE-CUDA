#!/usr/bin/env bash
# Permanent check: the Goose parser's arrays build a common_json tree that dumps like nlohmann.
#
# This is the adapter test: parse with the Goose door, turn the flat arrays into a common_json tree
# (tests/json/adapter/json_adapter.cpp), dump it with common_json itself, and compare the bytes with
# nlohmann's ordered dump of the same text. The dumper-only comparison lives in
# tests/json/nlohmann-shadow; this one exercises the tree bridge.
#
# Tier: fast - two translation units, no model, no GPU, a few seconds. Exit 77 without a library.
#
# Usage: bash tests/json/adapter/run.sh [lib-dir]
set -uo pipefail
D="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$D/../../.." && pwd)"
OUT=$D/build
LOG=$OUT/run.log
mkdir -p "$OUT"
. "$ROOT/tests/lib-default.sh"
BIN=${1:-$(lib_default "$ROOT")}

# The door has to exist in the library under test. On 2026-11-19 the released set
# (builds/checkpoint-20261119-EXP1476, 0ffd98c2) exports no json_parse at all, while the working
# library in builds/flint-acc/bin (bea5d6af, provenance unknown) does - so a pass against the working
# directory would say nothing about the shipped artefact.
if [ -f "$BIN/libllama.so.0" ] && ! nm -D "$BIN/libllama.so.0" 2>/dev/null | grep -qw json_parse; then
    printf 'skip: %s exports no json_parse door\n' "${BIN#$ROOT/}" | tee "$LOG"
    exit 77
fi
if [ ! -f "$BIN/libllama.so.0" ]; then
    printf 'skip: no libllama.so.0 under %s\n' "${BIN#$ROOT/}" | tee "$LOG"
    exit 77
fi

{
    printf 'library: %s\n' "${BIN#$ROOT/}"
    printf 'sha256:  %s\n' "$(sha256sum "$BIN/libllama.so.0" | cut -d' ' -f1)"
    printf 'source:  %s\n' "$(git -C "$ROOT" rev-parse --short HEAD 2>/dev/null || echo unknown)"
    printf '$ g++-13 -O2 -std=c++17 probe.cpp json.cpp -ldl\n'
    g++-13 -O2 -std=c++17 -I"$ROOT/vendor/flint/common" \
        -I"$ROOT/vendor/flint/vendor/nlohmann" -I"$ROOT/vendor/flint/vendor" \
        -I"$ROOT/vendor/flint/ggml/include" -I"$ROOT/vendor/flint/include" -I"$ROOT/vendor/flint/src" \
        "$D/probe.cpp" "$ROOT/vendor/flint/common/json.cpp" "$ROOT/vendor/flint/common/json-adapter.cpp" -o "$OUT/probe" -ldl || exit 1
    printf '$ probe %s/libllama.so.0\n' "${BIN#$ROOT/}"
    LD_LIBRARY_PATH="$BIN" "$OUT/probe" "$BIN/libllama.so.0"
} 2>&1 | tee "$LOG"
rc=${PIPESTATUS[0]}
printf '[exit %d]\n' "$rc" | tee -a "$LOG"
exit "$rc"
