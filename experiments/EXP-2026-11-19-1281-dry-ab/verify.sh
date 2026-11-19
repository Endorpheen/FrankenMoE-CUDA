#!/usr/bin/env bash
# Two processes, one switch, bitwise comparison of every logit.
set -uo pipefail
R=$(cd "$(dirname "$0")/../.." && pwd)
LIB=$R/builds/flint-acc/bin
D=$(dirname "$0")
BIN=/tmp/dry_ab
g++-13 -O2 -std=c++17 -w "$D/diff.cpp" -I "$R/vendor/flint/include" -I "$R/vendor/flint/ggml/include" -L "$LIB" -lllama \
    -Wl,-rpath,"$LIB" -o "$BIN" || exit 1
GGML_GOOSE_SAMPLER_DRY=1 "$BIN" /tmp/dry_goose.txt >/dev/null || exit 1
GGML_GOOSE_SAMPLER_DRY=0 "$BIN" /tmp/dry_native.txt >/dev/null || exit 1
if cmp -s /tmp/dry_goose.txt /tmp/dry_native.txt; then
    echo "dry ab: 200 cases, every logit bit-identical ($(wc -l < /tmp/dry_native.txt) lines)"
else
    echo "dry ab: MISMATCH"
    diff /tmp/dry_goose.txt /tmp/dry_native.txt | head -6
    exit 1
fi
