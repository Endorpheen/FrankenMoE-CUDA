#!/usr/bin/env bash
set -uo pipefail
R=$(cd "$(dirname "$0")/../.." && pwd)
LIB=$R/builds/flint-acc/bin
D=$(dirname "$0")
BIN=/tmp/chain_order
g++-13 -O2 -std=c++17 -w "$D/diff.cpp" -I "$R/vendor/flint/include" -I "$R/vendor/flint/ggml/include" \
    -L "$LIB" -lllama -Wl,-rpath,"$LIB" -o "$BIN" || exit 1
ON="GGML_GOOSE_SAMPLER_CHAIN=1 GGML_GOOSE_SAMPLER_TOP_P=1 GGML_GOOSE_SAMPLER_PENALTIES=1"
OFF="GGML_GOOSE_SAMPLER_CHAIN=0 GGML_GOOSE_SAMPLER_TOP_P=0 GGML_GOOSE_SAMPLER_PENALTIES=0"
env $ON "$BIN" /tmp/chain_goose.txt >/dev/null 2>/tmp/chain_goose.log || exit 1
env $OFF "$BIN" /tmp/chain_native.txt >/dev/null 2>&1 || exit 1
if cmp -s /tmp/chain_goose.txt /tmp/chain_native.txt; then
    echo "chain order: 120 rounds identical ($(grep -c '^[0-9]' /tmp/chain_native.txt) draws, doors: $(grep -cE 'FG171_(TOP_P|DIST|PENALTIES)' /tmp/chain_goose.log) counter lines)"
else
    echo "chain order: MISMATCH"
    diff /tmp/chain_goose.txt /tmp/chain_native.txt | head -6
    exit 1
fi
