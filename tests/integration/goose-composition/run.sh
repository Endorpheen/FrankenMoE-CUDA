#!/usr/bin/env bash
# Permanent check: every canonical module set still composes and typechecks together.
#
# The composed unit is the one artifact every stage shares, so a module that stops fitting its
# neighbours is caught here before any build. This delegates to tools/check-composition.sh, which
# owns the set list; the wrapper adds the log, the compiler pin it ran with and the exit verdict.
#
# Tier: integration - no model, no GPU, but the Goose compiler is needed.
# Exit 0 when all sets compose and typecheck; 1 otherwise.
#
# Usage: bash tests/integration/goose-composition/run.sh
set -uo pipefail
D="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$D/../../.." && pwd)"
GOOSE=${GOOSE:-$(python3 -c "import json;print(json.load(open('$ROOT/runtime/compiler/current.json'))['compiler'])" 2>/dev/null || true)}
OUT=$D/build
LOG=$OUT/run.log
mkdir -p "$OUT"

if [ -z "${GOOSE:-}" ] || [ ! -x "$GOOSE" ]; then
    printf 'skip: no Goose compiler at %s\n' "${GOOSE:-<unset>}" | tee "$LOG"
    exit 77
fi

{
    printf 'compiler: %s\n' "$GOOSE"
    printf 'sha256:   %s\n' "$(sha256sum "$GOOSE" | cut -d' ' -f1)"
    printf 'source:   %s\n' "$(git -C "$ROOT" rev-parse --short HEAD 2>/dev/null || echo unknown)"
    printf '$ GOOSE=%s bash tools/check-composition.sh\n' "$GOOSE"
    GOOSE="$GOOSE" bash "$ROOT/tools/check-composition.sh"
} 2>&1 | tee "$LOG"
rc=${PIPESTATUS[0]}
printf '[exit %d]\n' "$rc" | tee -a "$LOG"
exit "$rc"
