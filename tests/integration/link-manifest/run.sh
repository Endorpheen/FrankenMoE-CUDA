#!/usr/bin/env bash
# Permanent check: every dependency of a shipped set resolves, and no artefact advertises a build path.
#
# P10-06 asks for a compile/link manifest audit. Two facts matter to a release reader: nothing the
# set needs is absent, and nothing in it points at a directory that exists only on the build host.
#
# Tier: fast - reads a directory with readelf and nm, about a second. Exit 77 when the set is absent.
#
# Usage: bash tests/integration/link-manifest/run.sh [set-dir]
set -uo pipefail
D="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$D/../../.." && pwd)"
OUT=$D/build
LOG=$OUT/run.log
mkdir -p "$OUT"
SET=${1:-}

if [ -z "$SET" ]; then
    # the newest release set or checkpoint that carries a runnable server
    for c in /home/end0/AI/LlamaServer/frankenmoe-release-* "$ROOT"/builds/checkpoint-*; do
        [ -f "$c/llama-server" ] && [ -f "$c/libllama.so.0" ] && SET=$c
    done
fi
if [ -z "$SET" ] || [ ! -d "$SET" ]; then
    printf 'skip: no release set or checkpoint with a server found\n' | tee "$LOG"
    exit 77
fi

{
    printf 'set:    %s\n' "${SET#$ROOT/}"
    printf 'source: %s\n' "$(git -C "$ROOT" rev-parse --short HEAD 2>/dev/null || echo unknown)"
    printf '$ python3 tools/audit-link-manifest.py %s\n' "${SET#$ROOT/}"
    python3 "$ROOT/tools/audit-link-manifest.py" "$SET"
} 2>&1 | tee "$LOG"
rc=${PIPESTATUS[0]}
printf '[exit %d]\n' "$rc" | tee -a "$LOG"
exit "$rc"
