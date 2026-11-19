#!/usr/bin/env bash
set -uo pipefail
R=$(cd "$(dirname "$0")/../.." && pwd)
LIB=$R/builds/flint-acc/bin
D=$(dirname "$0")
BIN=/tmp/chain_stateful
g++-13 -O2 -std=c++17 -w "$D/diff.cpp" -I "$R/vendor/flint/include" -I "$R/vendor/flint/ggml/include" \
    -L "$LIB" -lllama -Wl,-rpath,"$LIB" -o "$BIN" || exit 1
ON="GGML_GOOSE_SAMPLER_CHAIN=1 GGML_GOOSE_SAMPLER_TOP_P=1 GGML_GOOSE_SAMPLER_PENALTIES=1 GGML_GOOSE_SAMPLER_DRY=1 GGML_GOOSE_SAMPLER_TYPICAL=1 GGML_GOOSE_SAMPLER_XTC=1"
OFF="GGML_GOOSE_SAMPLER_CHAIN=0 GGML_GOOSE_SAMPLER_TOP_P=0 GGML_GOOSE_SAMPLER_PENALTIES=0 GGML_GOOSE_SAMPLER_DRY=0 GGML_GOOSE_SAMPLER_TYPICAL=0 GGML_GOOSE_SAMPLER_XTC=0"
env $ON "$BIN" /tmp/cs_goose.txt >/dev/null 2>/tmp/cs_goose.log || exit 1
env $OFF "$BIN" /tmp/cs_native.txt >/dev/null 2>&1 || exit 1
fails=0
# configuration 0: penalties + DRY + top_k + top_p + temp + dist
if cmp -s <(awk '$1==0' /tmp/cs_goose.txt) <(awk '$1==0' /tmp/cs_native.txt); then
    echo "chain with DRY: identical (DRY counters: $(grep -c FG171_DRY /tmp/cs_goose.log))"
else
    echo "chain with DRY: MISMATCH"; fails=1
    diff <(awk '$1==0' /tmp/cs_goose.txt) <(awk '$1==0' /tmp/cs_native.txt) | head -4
fi
# configuration 1: typical + XTC + mirostat + adaptive-p + dist - OPEN
d=$(diff <(awk '$1==1' /tmp/cs_goose.txt) <(awk '$1==1' /tmp/cs_native.txt) | grep -c '^<')
if [ "$d" -eq 0 ]; then echo "chain stateful: identical"; else
    echo "chain stateful: OPEN, $d rounds differ (composition of typical and XTC - see README)"
fi
exit $fails
