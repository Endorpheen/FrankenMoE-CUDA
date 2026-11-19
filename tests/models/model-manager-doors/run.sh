#!/usr/bin/env bash
# Permanent check for the model manager's door decisions.
#
# The probe asks the shipped library for its own answers: mg_status_str, mg_source_str,
# mg_stop_gate, mg_unload_action and mg_remove_gate. A table over their inputs is what holds
# them until a running model exercises the call site (P8-06).
#
# Tier: fast - no model, no GPU. It needs the built library, because the doors live in it; when
# the library is absent the test skips (exit 77) instead of pretending to have checked.
#
# Usage: bash tests/models/model-manager-doors/run.sh
set -uo pipefail
D="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$D/../../.." && pwd)"
. "$ROOT/tests/lib-default.sh"
BIN=${APEX_SERVER_DIR:-$(lib_default "$ROOT")}
LIB=$BIN/libllama.so.0
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
    printf '$ gcc -O1 probe.c -ldl\n'
    gcc-13 -O1 -o "$OUT/probe" "$D/probe.c" -ldl || exit 1
    printf '$ LD_LIBRARY_PATH=%s probe %s\n' "${BIN#$ROOT/}" "${LIB#$ROOT/}"
    LD_LIBRARY_PATH="$BIN" "$OUT/probe" "$LIB"
} 2>&1 | tee "$LOG"
rc=${PIPESTATUS[0]}
if [ "$rc" -ne 0 ]; then
    printf '[exit %d] a model manager decision changed\n' "$rc" | tee -a "$LOG"
    exit 1
fi
printf '[exit 0] status, source, stop, remove and unload behave as the module states\n' | tee -a "$LOG"
exit 0
