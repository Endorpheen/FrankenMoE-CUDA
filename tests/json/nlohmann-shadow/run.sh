#!/usr/bin/env bash
# Permanent shadow comparison: the Goose JSON doors against nlohmann on the same texts.
#
# Both implementations are in one binary: nlohmann is the vendored header-only library, the Goose
# side is reached through the shipped library. The comparison is byte-for-byte on the dump, which is
# what catches the two promises common_json makes - key order and number normalisation.
#
# Tier: fast - one translation unit, no model, no GPU. Exit 77 without a library.
#
# Usage: bash tests/json/nlohmann-shadow/run.sh [lib-dir]
set -uo pipefail
D="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$D/../../.." && pwd)"
OUT=$D/build
LOG=$OUT/run.log
mkdir -p "$OUT"
BIN=${1:-${APEX_SERVER_DIR:-$ROOT/builds/flint-acc/bin}}

if [ ! -f "$BIN/libllama.so.0" ]; then
    printf 'skip: no libllama.so.0 under %s\n' "${BIN#$ROOT/}" | tee "$LOG"
    exit 77
fi

{
    printf 'library: %s\n' "${BIN#$ROOT/}"
    printf 'sha256:  %s\n' "$(sha256sum "$BIN/libllama.so.0" | cut -d' ' -f1)"
    printf 'source:  %s\n' "$(git -C "$ROOT" rev-parse --short HEAD 2>/dev/null || echo unknown)"
    printf '$ g++-13 -O2 -std=c++17 probe.cpp -ldl\n'
    g++-13 -O2 -std=c++17 -I"$ROOT/vendor/flint/vendor/nlohmann" -I"$ROOT/vendor/flint/vendor" \
        "$D/probe.cpp" -o "$OUT/probe" -ldl || exit 1
    printf '$ probe %s/libllama.so.0\n' "${BIN#$ROOT/}"
    LD_LIBRARY_PATH="$BIN" "$OUT/probe" "$BIN/libllama.so.0"
} 2>&1 | tee "$LOG"
rc=${PIPESTATUS[0]}
printf '[exit %d]\n' "$rc" | tee -a "$LOG"
exit "$rc"
