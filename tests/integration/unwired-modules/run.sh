#!/usr/bin/env bash
# Permanent check: no Goose module exports a decision and sits outside every composed unit.
#
# Twice in a row a module owned a decision, nothing contained it, and a host grew its own
# version of the rule instead (EXP995 shutdown-policy, EXP996 limits/error_404). This turns
# that accident into a list. Seconds to run.
#
# Tier: fast (it reads the tree, not a build) but registered as integration, because what it
# judges is the composition, not one module.
#
# Usage: bash tests/integration/unwired-modules/run.sh
set -uo pipefail
D="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$D/../../.." && pwd)"
OUT=$D/build
LOG=$OUT/run.log
mkdir -p "$OUT"
{
    printf 'source: %s\n' "$(git -C "$ROOT" rev-parse --short HEAD 2>/dev/null || echo unknown)"
    printf '$ python3 tools/check-unwired-modules.py\n'
    python3 "$ROOT/tools/check-unwired-modules.py"
} 2>&1 | tee "$LOG"
rc=${PIPESTATUS[0]}
printf '[exit %d]\n' "$rc" | tee -a "$LOG"
exit "$rc"
