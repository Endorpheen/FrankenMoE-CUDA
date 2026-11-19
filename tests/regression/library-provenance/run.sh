#!/usr/bin/env bash
# Is the working library the one that was accepted?
#
# Rule 22: accepted executable sets are immutable, and new builds live in their own directory. What
# that rule cannot prevent by itself is the working directory being rebuilt in place - which happened
# on 2026-11-19: builds/flint-acc/bin/libllama.so.0 changed at 19:18, after the 19:00 checkpoint, with
# no script under tools/ writing there, so every test pointed at it measured an artefact nobody
# accepted.
#
# This does not fail on a mismatch, because a working directory is allowed to hold a newer build. It
# makes the mismatch visible and names the artefact the release actually carries.
#
# Tier: fast - a few sha256sum calls.
#
# Usage: bash tests/regression/library-provenance/run.sh
set -uo pipefail
D="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$D/../../.." && pwd)"
OUT=$D/build
LOG=$OUT/run.log
mkdir -p "$OUT"
WORK=$ROOT/builds/flint-acc/bin/libllama.so.0

[ -f "$WORK" ] || { printf 'skip: no working library at builds/flint-acc/bin\n' | tee "$LOG"; exit 77; }

{
    printf 'working: %s\n' "${WORK#$ROOT/}"
    printf '          sha256 %s  mtime %s\n' \
        "$(sha256sum "$WORK" | cut -d' ' -f1)" "$(date -r "$WORK" +%Y-%m-%dT%H:%M)"
    printf '\naccepted copies:\n'
    found=0; match=0
    for c in /home/end0/AI/LlamaServer/frankenmoe-release-* "$ROOT"/builds/checkpoint-*; do
        [ -f "$c/libllama.so.0" ] || continue
        found=$((found + 1))
        h=$(sha256sum "$c/libllama.so.0" | cut -d' ' -f1)
        if [ "$h" = "$(sha256sum "$WORK" | cut -d' ' -f1)" ]; then
            printf '  MATCH  %-58s %s\n' "$(basename "$c")" "${h:0:16}"
            match=1
        else
            printf '  differ %-58s %s\n' "$(basename "$c")" "${h:0:16}"
        fi
    done
    if [ "$found" -eq 0 ]; then
        printf '  none found\n'
        exit 77
    fi
    if [ "$match" -eq 0 ]; then
        printf '\nNOTE: the working library matches no accepted copy. It is a working build, not the\n'
        printf '      accepted artefact: a measurement taken on it describes this directory, not a\n'
        printf '      release. Point tests at an accepted set with APEX_SERVER_DIR or use\n'
        printf '      tests/lib-default.sh, which prefers one.\n'
    fi
} 2>&1 | tee "$LOG"
rc=$?
printf '[exit %d]\n' "$rc" | tee -a "$LOG"
exit 0
