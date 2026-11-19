#!/usr/bin/env bash
# Permanent check: every composed unit still answers a question with a known answer.
#
# A module that stops answering - a symbol dropped in a rebuild, an init lost, a table not built -
# fails silently otherwise: the C++ falls back or the bridge returns a default, and only something
# far downstream notices. This asks the shipped library directly (tools/check-goose-modules.py).
#
# Caveat kept from the tool: calling fg_gr_check here prints a row count for FG_GR_REUSE, exactly
# as a model run would. A count appearing while running THIS test is not evidence that the profile
# reaches that variant; only counts printed during a server run are.
#
# Tier: integration - no model, no GPU, but the library must be built.
# Exit 77 when the library is absent.
#
# Usage: bash tests/integration/goose-modules/run.sh
set -uo pipefail
D="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$D/../../.." && pwd)"
LIB=${APEX_SERVER_DIR:-$ROOT/builds/flint-acc/bin}/libllama.so.0
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
    printf '$ python3 tools/check-goose-modules.py %s\n' "${LIB#$ROOT/}"
    python3 "$ROOT/tools/check-goose-modules.py" "$LIB"
} 2>&1 | tee "$LOG"
rc=${PIPESTATUS[0]}
printf '[exit %d]\n' "$rc" | tee -a "$LOG"
exit "$rc"
