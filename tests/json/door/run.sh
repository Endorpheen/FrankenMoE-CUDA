#!/usr/bin/env bash
# Permanent check: the ported JSON parser answers on the shipped library.
#
# Equality with nlohmann is already proved (78 parse cases over 81 850 values, 188 dump cases).
# What this pins is that the door is composed and reachable on the path the adapter will use:
# every valid text parses (return 0) with its own root kind, and every invalid text is refused
# (-1) rather than silently accepted (P10-07).
#
# Tier: fast - no model, no GPU. Needs the built library; skips (exit 77) without it.
#
# Usage: bash tests/json/door/run.sh
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
    printf '[exit %d] the composed parser refuses valid text or accepts invalid text\n' "$rc" | tee -a "$LOG"
    exit 1
fi
printf '[exit 0] the ported parser is composed and answers on the shipped library\n' | tee -a "$LOG"
exit 0
