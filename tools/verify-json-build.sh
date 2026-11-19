#!/usr/bin/env bash
# Accept a build for the JSON path, in one pass.
#
# P10-07 spent a day in a loop because the halves lived in different artefacts: the adapter in
# libllama-common, the Goose parser in libllama, and every check looked at one of them. This runs the three
# checks that together answer the whole question, in the order that makes a failure obvious:
#
#   1. the door json_parse and the adapter fg_json_adapter::to_tree are both in the same set;
#   2. the swap test passes in all three modes - strict mode makes a decline an error, so a passing run
#      means the Goose parser produced the trees, not nlohmann;
#   3. the set is self-contained, because a set that cannot load has not been verified.
#
# Usage: bash tools/verify-json-build.sh <set-dir>
# Exit 0 when all three pass.
set -uo pipefail
ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
SET=${1:?usage: verify-json-build.sh <set-dir>}
fail=0

echo "set: $SET"
LIB=$SET/libllama.so.0
COMMON=$SET/libllama-common.so.0

printf '%-34s ' "door json_parse in libllama"
SYMS=$(nm -D --defined-only "$LIB" 2>/dev/null || true)
case "$SYMS" in
    *" json_parse"*) echo "present";;
    *) echo "ABSENT"; fail=1;;
esac

printf '%-34s ' "adapter in libllama-common"
CSYMS=$(nm -D --defined-only "$COMMON" 2>/dev/null | c++filt || true)
case "$CSYMS" in
    *"fg_json_adapter::to_tree"*) echo "present";;
    *) echo "ABSENT"; fail=1;;
esac

printf '%-34s ' "set self-contained"
if bash "$ROOT/tests/regression/set-self-contained/run.sh" "$SET" >/tmp/vjb-sc.$$ 2>&1; then
    echo "ok"
else
    echo "FAIL"; sed -n '1,6p' /tmp/vjb-sc.$$; fail=1
fi

printf '%-34s ' "swap test, three modes"
if bash "$ROOT/tests/json/swap/run.sh" "$SET" >/tmp/vjb-sw.$$ 2>&1; then
    echo "ok"
    grep -h "json swap:" /tmp/vjb-sw.$$ | tail -1
else
    echo "FAIL"; grep -hE "json swap:|skip:|FAIL" /tmp/vjb-sw.$$ | head -4; fail=1
fi

rm -f /tmp/vjb-sc.$$ /tmp/vjb-sw.$$
echo
[ "$fail" -eq 0 ] && echo "json path: accepted by all three checks" || echo "json path: NOT accepted"
exit "$fail"
