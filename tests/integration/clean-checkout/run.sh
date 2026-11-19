#!/usr/bin/env bash
# Permanent check: a clean checkout composes and typechecks with the compiler the pin names.
#
# P10-06 asks for a clean isolated build with the pinned compiler. The cheap half of it - that the
# sources are self-sufficient and the pin resolves - costs seconds and is checked here; the full
# build is tools/verify-clean-build.sh without --compose-only, which is the owner's command because
# it compiles the whole unit.
#
# Tier: fast - a fresh worktree, compose and typecheck, about two seconds.
#
# Usage: bash tests/integration/clean-checkout/run.sh
set -uo pipefail
D="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$D/../../.." && pwd)"
OUT=$D/build
LOG=$OUT/run.log
mkdir -p "$OUT"
{
    printf '$ bash tools/verify-clean-build.sh --compose-only\n'
    bash "$ROOT/tools/verify-clean-build.sh" --compose-only
} 2>&1 | tee "$LOG"
rc=${PIPESTATUS[0]}
printf '[exit %d]\n' "$rc" | tee -a "$LOG"
exit "$rc"
