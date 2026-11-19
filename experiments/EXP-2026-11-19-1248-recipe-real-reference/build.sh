#!/usr/bin/env bash
# Rebuild and run the recipe differential against the real native registry.
set -euo pipefail
R=$(cd "$(dirname "$0")/../.." && pwd)
cmake -S "$R/runtime" -B "$R/builds/bmoe" -DCMAKE_BUILD_TYPE=Release >/dev/null
cmake --build "$R/builds/bmoe" -j 8 >/dev/null
g++-13 -O2 -std=c++17 -w -I "$R/runtime" "$(dirname "$0")/diff.cpp" \
    "$R/builds/bmoe/core/libbmoe_core.a" -ldl -o /tmp/recipe_diff
/tmp/recipe_diff
