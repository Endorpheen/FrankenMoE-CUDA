#!/usr/bin/env bash
# The writer cases this experiment accepted, run against the SHIPPED library instead of the module's own
# build. The behaviour was accepted here; this checks it survived into the cumulative artefact, which is
# the question rule 33 asks and a compile cannot answer.
set -uo pipefail
R=$(cd "$(dirname "$0")/../.." && pwd)
D=$(dirname "$0")
LIB=$R/builds/flint-acc/bin
g++-13 -O2 -std=c++17 -w "$D/tests/fixture.cpp" -I "$D/build" -I "$R/vendor/flint/ggml/include" \
    -L "$LIB" -lllama -lggml-base -Wl,-rpath,"$LIB" -o /tmp/writer_vs_shipped || exit 1
out=$(/tmp/writer_vs_shipped 2>&1) || { echo "$out" | tail -3; exit 1; }
echo "$out" | tail -1
