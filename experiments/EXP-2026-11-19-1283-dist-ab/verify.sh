#!/usr/bin/env bash
set -uo pipefail
R=$(cd "$(dirname "$0")/../.." && pwd)
LIB=$R/builds/flint-acc/bin
D=$(dirname "$0")
BIN=/tmp/dist_ab
g++-13 -O2 -std=c++17 -w "$D/diff.cpp" -I "$R/vendor/flint/include" -I "$R/vendor/flint/ggml/include" \
    -L "$LIB" -lllama -Wl,-rpath,"$LIB" -o "$BIN" || exit 1
GGML_GOOSE_SAMPLER_CHAIN=1 "$BIN" /tmp/dist_goose.txt >/dev/null 2>/tmp/dist_goose.log || exit 1
GGML_GOOSE_SAMPLER_CHAIN=0 "$BIN" /tmp/dist_native.txt >/dev/null 2>&1 || exit 1
calls=$(grep -c "FG171_DIST" /tmp/dist_goose.log || true)
if cmp -s /tmp/dist_goose.txt /tmp/dist_native.txt; then
    echo "dist: 8000 draws identical, door reached ($calls counter lines)"
else
    echo "dist: MISMATCH"
    diff /tmp/dist_goose.txt /tmp/dist_native.txt | head -4
    exit 1
fi
