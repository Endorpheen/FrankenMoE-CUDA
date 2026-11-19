#!/usr/bin/env bash
# Permanent check: every Goose symbol the compiled carriers reference has a definition.
#
# The runtime compiles thirteen carrier sources from runtime/reconstructed/src and the rest of
# vendor/flint/src, and the two copies of a carrier are not interchangeable (see
# docs/goose-migration/CARRIER-DIVERGENCE-20261119.md). This asks the only question the link cares
# about - is every referenced symbol defined by some compiled source - in a fifth of a second.
#
# Tier: integration. It reads sources, needs no build, no model and no GPU.
#
# Usage: bash tests/integration/carrier-symbols/run.sh
set -uo pipefail
D="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$D/../../.." && pwd)"
OUT=$D/build
LOG=$OUT/run.log
mkdir -p "$OUT"
{
    printf 'source: %s\n' "$(git -C "$ROOT" rev-parse --short HEAD 2>/dev/null || echo unknown)"
    printf '$ python3 runtime/reconstructed/check_carriers.py\n'
    python3 "$ROOT/runtime/reconstructed/check_carriers.py" --root "$ROOT"
} 2>&1 | tee "$LOG"
rc=${PIPESTATUS[0]}
printf '[exit %d]\n' "$rc" | tee -a "$LOG"
exit "$rc"
