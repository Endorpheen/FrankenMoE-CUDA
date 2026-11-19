#!/usr/bin/env bash
set -euo pipefail
R=$(cd "$(dirname "$0")/../.." && pwd)
D="$(dirname "$0")"
cmake -S "$R/runtime" -B "$R/builds/bmoe" -DCMAKE_BUILD_TYPE=Release >/dev/null
cmake --build "$R/builds/bmoe" -j 8 >/dev/null
TMPDIR=$R/experiments/EXP-2026-11-18-1008-p1-12-gemv/tmp GOOSE_HOME="$R" \
    /home/end0/AI/Fried-Goose/.cache/goose/simd7 -o /tmp/cfg.c "$D/src/validate.goose"
gcc-13 -O2 -std=c11 -w -Dmain=fg_cfg_main -c "$D/tests/wrap.c" -o /tmp/cfgwrap.o
g++-13 -O2 -std=c++17 -w -I "$R/runtime" "$D/tests/gate.cpp" /tmp/cfgwrap.o \
    "$R/builds/bmoe/core/libbmoe_core.a" -o /tmp/cfg_gate
/tmp/cfg_gate
