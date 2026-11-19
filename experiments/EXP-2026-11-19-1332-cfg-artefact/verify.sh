#!/usr/bin/env bash
# The config-validation rules checked against the SHIPPED library's cfg_validate, with the two fields the
# original gate could not express (moe.drop_cold_frac, moe.substitute_lambda) now randomised.
set -uo pipefail
R=$(cd "$(dirname "$0")/../.." && pwd)
D=$(dirname "$0")
LIB=$R/builds/flint-acc/bin
[ -f "$R/builds/bmoe/core/libbmoe_core.a" ] || { echo "bmoe_core missing"; exit 2; }
gcc-13 -O2 -c "$D/wrap_now.c" -o /tmp/cfgwrap_now.o || exit 1
g++-13 -O2 -std=c++17 -w -I "$R/runtime" "$D/gate.cpp" /tmp/cfgwrap_now.o \
    "$R/builds/bmoe/core/libbmoe_core.a" -L "$LIB" -lllama -Wl,-rpath,"$LIB" -o /tmp/cfg_artifact || exit 1
out=$(/tmp/cfg_artifact 2>&1) || { echo "$out" | tail -3; exit 1; }
echo "$out" | tail -1
