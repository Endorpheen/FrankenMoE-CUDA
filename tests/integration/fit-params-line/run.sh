#!/usr/bin/env bash
# Permanent check: the fit-params line the port produces equals the native format.
#
# Oracle: the line is written out from the tool's own printf
#   printf("-c %" PRIu32 " -ngl %" PRIi32, cparams.n_ctx, mparams.n_gpu_layers)
# plus " -ts " and the tensor split when more than one device. The fit-params target is not in
# this build set, so the rule is re-expressed from its source and both sides are fed the same
# numbers - the same discipline the sampler doors use. 16 cases, four contexts x four layer counts.
#
# Tier: fast - compiles one small C file, no model, no GPU. Exit 77 without a library.
#
# Usage: bash tests/integration/fit-params-line/run.sh [lib-dir]
set -uo pipefail
D="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$D/../../.." && pwd)"
OUT=$D/build
LOG=$OUT/run.log
mkdir -p "$OUT"

BIN=${1:-${APEX_SERVER_DIR:-$ROOT/builds/flint-acc/bin}}
if [ ! -f "$BIN/libllama.so.0" ]; then
    printf 'skip: no libllama.so.0 under %s\n' "${BIN#$ROOT/}" | tee "$LOG"
    exit 77
fi

{
    printf 'library: %s\n' "${BIN#$ROOT/}"
    printf 'sha256:  %s\n' "$(sha256sum "$BIN/libllama.so.0" | cut -d' ' -f1)"
    printf 'source:  %s\n' "$(git -C "$ROOT" rev-parse --short HEAD 2>/dev/null || echo unknown)"
    printf '$ gcc -O2 diff.c -ldl\n'
    gcc-13 -O2 -o "$OUT/fp_diff" "$D/diff.c" -ldl || exit 1
    printf '$ fp_diff %s/libllama.so.0\n' "${BIN#$ROOT/}"
    LD_LIBRARY_PATH="$BIN" "$OUT/fp_diff" "$BIN/libllama.so.0"
} 2>&1 | tee "$LOG"
rc=${PIPESTATUS[0]}
grep -q "disagreements=0" "$LOG" || rc=1
printf '[exit %d]\n' "$rc" | tee -a "$LOG"
exit "$rc"
