#!/usr/bin/env bash
# Verify a candidate build directory in seconds, before anyone measures it.
#
# Every failure in this project that cost a rebuild had the same shape: the artefact was not what the
# person thought it was. A library built from the wrong carrier, a checkpoint missing the doors a test
# needed, a working directory nobody could attribute. This script answers those questions in one pass
# and prints a table, so a candidate is accepted or rejected on evidence rather than on the sound of a
# successful link.
#
# Usage: bash tools/verify-candidate.sh <dir>
#        <dir> is a build output directory: it may hold the libraries directly (a run directory) or
#        under bin/ (a cmake build).
#
# Exit 0 when every check passes, 1 otherwise. Seconds, no model, no GPU.
set -uo pipefail
ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
DIR=${1:?usage: verify-candidate.sh <dir>}
fail=0

for cand in "$DIR" "$DIR/run" "$DIR/bin" "$DIR/support/bin"; do
    [ -f "$cand/libllama.so.0" ] && { LIB=$cand/libllama.so.0; break; }
done
[ -n "${LIB:-}" ] || { echo "no libllama.so.0 under $DIR"; exit 1; }
LIB=$(cd "$(dirname "$LIB")" && pwd)/libllama.so.0   # absolute, so the class comparison below means something
echo "candidate: $LIB"
echo "sha256:    $(sha256sum "$LIB" | cut -d' ' -f1)"
echo "source:    $(git -C "$ROOT" rev-parse --short HEAD)$(git -C "$ROOT" diff --quiet || echo ' + uncommitted')"
echo

printf '%-42s ' "accepted components present"
if python3 "$ROOT/tools/check-accepted-components.py" "$(dirname "$LIB")" >/tmp/vc-acc.$$ 2>&1; then
    echo "ok"
else
    echo "FAIL"; sed -n '1,12p' /tmp/vc-acc.$$; fail=1
fi

printf '%-42s ' "doors carried by the unit"
python3 "$ROOT/tools/unit-carry.py" "$LIB" 2>/dev/null | tail -3
python3 "$ROOT/tools/unit-carry.py" "$LIB" >/tmp/vc-carry.$$ 2>&1 || fail=1
grep -q "exported by this artefact:.*(0%)" /tmp/vc-carry.$$ && { echo "FAIL: the unit exported nothing"; fail=1; }

# nm output is captured first and grepped second, on purpose: `nm ... | grep -q` lets grep exit on the
# first match, nm die of SIGPIPE, and `set -o pipefail` turn a found symbol into a failed check. That
# mistake reported json_parse as absent from a library that defines it.
SYMS=$(nm -D --defined-only "$LIB" 2>/dev/null)

printf '%-42s ' "duplicate global symbols"
DUP=$(printf '%s\n' "$SYMS" | awk 'NF==3 {print $3}' | sort | uniq -d | head -5)
if [ -z "$DUP" ]; then
    echo "ok ($(printf '%s\n' "$SYMS" | awk 'NF==3' | wc -l) defined)"
else
    echo "FAIL: $(printf '%s' "$DUP" | tr '\n' ' ')"; fail=1
fi

# The JSON adapter is not a Goose door called json_parse: it is a C++ entry point, fg_json_adapter::to_tree,
# and it lives in libllama-common, not in libllama. Checking only libllama for the wrong name reported
# "absent" for a build that carries it - the same false negative this script exists to prevent.
printf '%-42s ' "JSON adapter"
COMMON=$(dirname "$LIB")/libllama-common.so.0
# Captured first, grepped second: `nm | c++filt | grep -q` lets grep exit on the first match, the
# upstream processes die of SIGPIPE and `set -o pipefail` turns a found symbol into a failed check.
# That is the third time this exact shape produced a false negative today.
COMMON_SYMS=$( [ -f "$COMMON" ] && nm -D --defined-only "$COMMON" 2>/dev/null | c++filt || true )
case "$COMMON_SYMS" in
    *"fg_json_adapter::to_tree"*) JSON_ADAPTER="present in $(basename "$COMMON") (fg_json_adapter::to_tree)" ;;
    *) JSON_ADAPTER="absent (P10-07 not in this build)" ;;
esac
printf '%s\n' "$JSON_ADAPTER"

printf '%-42s ' "artefact class"
if [ "$(dirname "$LIB")" = "$ROOT/builds/flint-acc/bin" ]; then
    echo "working directory - not an accepted set (rule 22)"
else
    echo "candidate directory: $(dirname "$LIB")"
fi
echo

rm -f /tmp/vc-acc.$$ /tmp/vc-carry.$$ /tmp/vc-dup.$$
[ "$fail" -eq 0 ] && echo "candidate: ok" || echo "candidate: FAILED"
exit "$fail"
