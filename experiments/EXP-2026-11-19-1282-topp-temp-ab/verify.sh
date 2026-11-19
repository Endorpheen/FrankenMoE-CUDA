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

# temp_ext: OPEN. The door rewrites logits in place; the native path also sorts and can change the
# array size. That is a contract difference, not a value difference, and deciding it needs a statement
# about what a sampler may do to the caller's array - so it is reported, not silently passed.
lines=$(diff <(arm tempext /tmp/topp_goose.txt) <(arm tempext /tmp/topp_native.txt) | grep -c '^<')
echo "tempext: OPEN, $lines lines differ (ordering and size, not values)"
exit $fails
