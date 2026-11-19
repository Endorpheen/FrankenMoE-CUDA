#!/usr/bin/env bash
set -euo pipefail
R=$(cd "$(dirname "$0")/../.." && pwd)
D="$(dirname "$0")"
cmake -S "$R/runtime" -B "$R/builds/bmoe" -DCMAKE_BUILD_TYPE=Release >/dev/null
cmake --build "$R/builds/bmoe" -j 8 >/dev/null
g++-13 -O2 -std=c++17 -w -I "$R/runtime" -I "$R/vendor/flint/ggml/include" \
    -I "$R/vendor/flint/ggml/src" -I "$R/runtime/core/include" "$D/diff.cpp" \
    "$R/builds/bmoe/core/libbmoe_core.a" "$R/builds/flint-acc/bin/libggml-base.so" \
    "$R/builds/flint-acc/bin/libggml-cpu.so" "$R/builds/flint-acc/bin/libggml.so" \
    -Wl,-rpath,"$R/builds/flint-acc/bin" -ldl -o /tmp/mx_diff
LD_LIBRARY_PATH="$R/builds/flint-acc/bin" /tmp/mx_diff
