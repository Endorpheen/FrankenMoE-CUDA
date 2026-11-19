#!/usr/bin/env bash
# Two arms with different verdicts, kept in one place so neither hides the other.
set -uo pipefail
R=$(cd "$(dirname "$0")/../.." && pwd)
LIB=$R/builds/flint-acc/bin
D=$(dirname "$0")
BIN=/tmp/topp_ab
g++-13 -O2 -std=c++17 -w "$D/diff.cpp" -I "$R/vendor/flint/include" -I "$R/vendor/flint/ggml/include" \
    -L "$LIB" -lllama -Wl,-rpath,"$LIB" -o "$BIN" || exit 1
GGML_GOOSE_SAMPLER_TOP_P=1 GGML_GOOSE_SAMPLER_CHAIN=1 "$BIN" /tmp/topp_goose.txt >/dev/null 2>&1 || exit 1
GGML_GOOSE_SAMPLER_TOP_P=0 GGML_GOOSE_SAMPLER_CHAIN=0 "$BIN" /tmp/topp_native.txt >/dev/null 2>&1 || exit 1

fails=0
# top-p: values, order and size must all match
arm() { awk -v a="$1" '$1==a {on=1; print; next} /^[a-z]/ {on=0} on' "$2"; }
if cmp -s <(arm topp /tmp/topp_goose.txt) <(arm topp /tmp/topp_native.txt); then
    echo "topp: bit-identical including order and size"
else
    echo "topp: MISMATCH"; fails=1
fi

# temp_ext: the same comparison. It was previously reported OPEN on an unsorted input, where the
# native's own sort looked like a difference; with the input the chain actually provides, the two
# runs are identical.
if cmp -s <(arm tempext /tmp/topp_goose.txt) <(arm tempext /tmp/topp_native.txt); then
    echo "tempext: bit-identical including order and size"
else
    echo "tempext: MISMATCH"; fails=1
fi
exit $fails
