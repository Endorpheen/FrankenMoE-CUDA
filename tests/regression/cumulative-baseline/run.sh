#!/usr/bin/env bash
# Permanent check (rule 31): the cumulative build carries every accepted component.
#
# The other half of the release pre-flight: the three named primitives and the Goose kernel
# object must be present before any acceptance measurement is taken. Seconds to run.
#
# Tier: fast. Exit 77 when the directory is absent.
#
# Usage: bash tests/regression/cumulative-baseline/run.sh [dir]
set -uo pipefail
D="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$D/../../.." && pwd)"
# Default: the newest checkpoint that carries the Goose kernel object. builds/flint-acc/bin does
# not carry libmoe_mmid_tiled.so, which is a recorded packaging gap rather than a lost component;
# a directory without it would fail this check for that reason and not for the one it tests.
default_dir() {
    if [ -n "${APEX_SERVER_DIR:-}" ]; then printf '%s' "$APEX_SERVER_DIR"; return; fi
    local d newest=""
    for d in "$ROOT"/builds/checkpoint-* /home/end0/AI/LlamaServer/frankenmoe-release-*; do
        [ -f "$d/libmoe_mmid_tiled.so" ] && [ -f "$d/libllama.so.0" ] && newest=$d
    done
    printf '%s' "${newest:-$ROOT/builds/flint-acc/bin}"
}
DIR=${1:-$(default_dir)}
OUT=$D/build
LOG=$OUT/run.log
mkdir -p "$OUT"

if [ ! -d "$DIR" ]; then
    printf 'skip: %s does not exist\n' "${DIR#$ROOT/}" | tee "$LOG"
    exit 77
fi

{
    printf 'library dir: %s\n' "${DIR#$ROOT/}"
    printf 'source:      %s\n' "$(git -C "$ROOT" rev-parse --short HEAD 2>/dev/null || echo unknown)"
    printf '$ python3 tools/check-cumulative-baseline.py %s\n' "${DIR#$ROOT/}"
    python3 "$ROOT/tools/check-cumulative-baseline.py" "$DIR"
} 2>&1 | tee "$LOG"
rc=${PIPESTATUS[0]}
printf '[exit %d]\n' "$rc" | tee -a "$LOG"
exit "$rc"
