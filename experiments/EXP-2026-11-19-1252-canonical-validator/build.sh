#!/usr/bin/env bash
set -euo pipefail
R=$(cd "$(dirname "$0")/../.." && pwd)
D="$(dirname "$0")"
cmake -S "$R/runtime" -B "$R/builds/bmoe" -DCMAKE_BUILD_TYPE=Release >/dev/null
cmake --build "$R/builds/bmoe" -j 8 >/dev/null
g++-13 -O2 -std=c++17 -w -I "$R/runtime" "$D/diff.cpp" "$R/builds/bmoe/core/libbmoe_core.a" \
    -ldl -o /tmp/canon_gate
/tmp/canon_gate
