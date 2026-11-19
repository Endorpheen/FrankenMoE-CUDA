#!/usr/bin/env bash
# Permanent check: the shipped library carries every accepted Goose component.
#
# Component that disappears in a rebuild fails silently downstream - the host falls back
# and only a much later symptom shows. This is the release pre-flight, unchanged, run in
# seconds. DIR defaults to the accepted library directory; a release set can be passed as
# the first argument.
#
# Tier: fast. Exit 77 when the directory or its libllama is absent.
#
# Usage: bash tests/regression/accepted-components/run.sh [dir]
set -uo pipefail
D="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$D/../../.." && pwd)"
DIR=${1:-$ROOT/builds/flint-acc/bin}
OUT=$D/build
LOG=$OUT/run.log
mkdir -p "$OUT"

if [ ! -e "$DIR/libllama.so.0" ] && [ ! -e "$DIR/libllama.so" ]; then
    printf 'skip: no libllama under %s\n' "${DIR#$ROOT/}" | tee "$LOG"
    exit 77
fi

{
    printf 'library dir: %s\n' "${DIR#$ROOT/}"
    printf 'source:      %s\n' "$(git -C "$ROOT" rev-parse --short HEAD 2>/dev/null || echo unknown)"
    printf '$ python3 tools/check-accepted-components.py %s\n' "${DIR#$ROOT/}"
    python3 "$ROOT/tools/check-accepted-components.py" "$DIR"
} 2>&1 | tee "$LOG"
rc=${PIPESTATUS[0]}
printf '[exit %d]\n' "$rc" | tee -a "$LOG"
exit "$rc"
