#!/usr/bin/env bash
# Permanent check that the JSON parser and dumper round-trip the canonical text.
#
# The parser and the dumper cannot share one composed unit (duplicate constants), so this is the
# check that exercises both together through the shipped library: parse a text, dump the tree
# compactly, require exactly the canonical bytes. Order of object keys matters, which is what
# common_json promises, and numbers are normalised the way nlohmann normalises them (P10-07).
#
# Tier: fast - no model, no GPU. It needs the built library; when it is absent the test skips
# (exit 77) instead of pretending to have checked.
#
# Usage: bash tests/json/roundtrip/run.sh
set -uo pipefail
D="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$D/../../.." && pwd)"
BIN=${APEX_SERVER_DIR:-$ROOT/builds/flint-acc/bin}
LIB=$BIN/libllama.so.0
OUT=$D/build
LOG=$OUT/run.log
mkdir -p "$OUT"

if [ ! -f "$LIB" ]; then
    printf 'skip: %s is not built\n' "${LIB#$ROOT/}" | tee "$LOG"
    exit 77
fi

{
    printf 'library: %s\n' "${LIB#$ROOT/}"
    printf 'sha256:  %s\n' "$(sha256sum "$LIB" | cut -d' ' -f1)"
    printf 'source:  %s\n' "$(git -C "$ROOT" rev-parse --short HEAD 2>/dev/null || echo unknown)"
    printf '$ gcc -O1 probe.c -ldl\n'
    gcc-13 -O1 -o "$OUT/probe" "$D/probe.c" -ldl || exit 1
    printf '$ LD_LIBRARY_PATH=%s probe %s\n' "${BIN#$ROOT/}" "${LIB#$ROOT/}"
    LD_LIBRARY_PATH="$BIN" "$OUT/probe" "$LIB"
} 2>&1 | tee "$LOG"
rc=${PIPESTATUS[0]}
if [ "$rc" -ne 0 ]; then
    printf '[exit %d] parse and dump do not round-trip\n' "$rc" | tee -a "$LOG"
    exit 1
fi
printf '[exit 0] both units round-trip the canonical text, key order included\n' | tee -a "$LOG"
exit 0
