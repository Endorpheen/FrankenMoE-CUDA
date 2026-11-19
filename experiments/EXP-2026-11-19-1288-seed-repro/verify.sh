#!/usr/bin/env bash
# Reproducibility of a fixed seed, through the shipped library and the doors.
#
# Three claims, each checked rather than assumed:
#   1. the same seed gives the same tokens twice - reproducibility;
#   2. a different seed gives different tokens - the seed is actually used, not ignored;
#   3. both hold with the doors ON, which is the shipping configuration.
set -uo pipefail
R=$(cd "$(dirname "$0")/../.." && pwd)
LIB=$R/builds/flint-acc/bin
D=$(dirname "$0")
BIN=/tmp/seed_repro
g++-13 -O2 -std=c++17 -w "$D/diff.cpp" -I "$R/vendor/flint/include" -I "$R/vendor/flint/ggml/include" \
    -L "$LIB" -lllama -Wl,-rpath,"$LIB" -o "$BIN" || exit 1
ON="GGML_GOOSE_SAMPLER_CHAIN=1 GGML_GOOSE_SAMPLER_TOP_P=1 GGML_GOOSE_SAMPLER_PENALTIES=1"
fails=0
env $ON "$BIN" 4242 /tmp/seed_a1.txt >/dev/null 2>&1 || exit 1
env $ON "$BIN" 4242 /tmp/seed_a2.txt >/dev/null 2>&1 || exit 1
env $ON "$BIN" 4243 /tmp/seed_b1.txt >/dev/null 2>&1 || exit 1
if cmp -s /tmp/seed_a1.txt /tmp/seed_a2.txt; then echo "same seed twice: identical ($(grep -c . /tmp/seed_a1.txt) rounds)"
else echo "same seed twice: DIFFERENT"; fails=1; fi
if cmp -s /tmp/seed_a1.txt /tmp/seed_b1.txt; then echo "different seed: identical - the seed is ignored"; fails=1
else echo "different seed: different tokens ($(diff /tmp/seed_a1.txt /tmp/seed_b1.txt | grep -c '^<') rounds)"; fi
exit $fails
