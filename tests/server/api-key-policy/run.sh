#!/usr/bin/env bash
# Permanent check: the API-key door answers like the middleware it replaces.
#
# server-http.cpp's middleware skips when no keys are configured or the path is public, takes
# Authorization and then X-Api-Key, strips a "Bearer " prefix and accepts a configured key. The Goose
# door key_authorize does the same in one pass; this asks it the same seven questions.
#
# Tier: fast - one C file, no model, no GPU. Exit 77 when the shipped library does not export the
# door, which is the case for the accepted set of 2026-11-19 (recorded in the README).
#
# Usage: bash tests/server/api-key-policy/run.sh [lib-dir]
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
if ! nm -D "$BIN/libllama.so.0" 2>/dev/null | grep -qw key_authorize; then
    printf 'skip: this library exports no key_authorize door\n' | tee "$LOG"
    printf '      the accepted set of 2026-11-19 predates the api-key module reaching the unit\n' | tee -a "$LOG"
    exit 77
fi

{
    printf 'library: %s\n' "${BIN#$ROOT/}"
    printf 'sha256:  %s\n' "$(sha256sum "$BIN/libllama.so.0" | cut -d' ' -f1)"
    printf 'source:  %s\n' "$(git -C "$ROOT" rev-parse --short HEAD 2>/dev/null || echo unknown)"
    printf '$ gcc -O1 probe.c -ldl\n'
    gcc-13 -O1 -o "$OUT/probe" "$D/probe.c" -ldl || exit 1
    LD_LIBRARY_PATH="$BIN" "$OUT/probe" "$BIN/libllama.so.0"
} 2>&1 | tee "$LOG"
rc=${PIPESTATUS[0]}
printf '[exit %d]\n' "$rc" | tee -a "$LOG"
exit "$rc"
