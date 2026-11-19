#!/usr/bin/env bash
# Permanent check (rule 33): nothing that existed in the previous accepted build has disappeared.
#
# A list of known components is a promise someone maintains; a symbol diff is a fact about two
# artefacts. Compares exported symbols and diagnostic strings of the previous checkpoint with the
# current build. Seconds to run.
#
# Tier: fast. Exit 77 when there is nothing to compare.
#
# Usage: bash tests/regression/symbol-loss/run.sh [current-lib]
set -uo pipefail
D="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$D/../../.." && pwd)"
OUT=$D/build
LOG=$OUT/run.log
mkdir -p "$OUT"

CUR=${1:-}
if [ -z "$CUR" ]; then
    for d in /home/end0/AI/LlamaServer/frankenmoe-release-* "$ROOT"/builds/checkpoint-*; do
        [ -f "$d/libllama.so.0" ] && CUR=$d/libllama.so.0
    done
fi
if [ -z "$CUR" ] || [ ! -e "$CUR" ]; then
    printf 'skip: no current library to compare\n' | tee "$LOG"
    exit 77
fi

{
    printf 'current: %s\n' "${CUR#$ROOT/}"
    printf 'source:  %s\n' "$(git -C "$ROOT" rev-parse --short HEAD 2>/dev/null || echo unknown)"
    printf '$ python3 tools/check-no-symbol-loss.py %s\n' "${CUR#$ROOT/}"
    python3 "$ROOT/tools/check-no-symbol-loss.py" "$CUR"
} 2>&1 | tee "$LOG"
rc=${PIPESTATUS[0]}
printf '[exit %d]\n' "$rc" | tee -a "$LOG"
exit "$rc"
