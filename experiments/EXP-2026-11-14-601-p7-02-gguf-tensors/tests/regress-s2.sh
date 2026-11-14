#!/bin/bash
# S2 regression: run the accepted S2 gate (EXP-2026-11-14-596) against the S1 +
# S2 content of source/gguf-tensors.goose. The S2 fixture only needs s1_* and
# s2_write, so the new file is compiled under the old name in a scratch tree.
set -euo pipefail

SUB=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
REPO=${FG_REPO:-/home/end0/AI/FrankenMoE-CUDA}
S2=${FG_S2:-$REPO/experiments/EXP-2026-11-14-596-p7-02-gguf-writer}
OUT=${FG_OUT:-$SUB/build/regress-s2}

rm -rf "$OUT"
mkdir -p "$OUT/source" "$OUT/tests"
cp "$S2/tests/fixture.cpp" "$S2/tests/build.sh" "$OUT/tests/"
cp "$SUB/source/gguf-tensors.goose" "$OUT/source/gguf-writer.goose"
chmod +x "$OUT/tests/build.sh"

bash "$OUT/tests/build.sh"
