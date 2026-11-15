#!/bin/bash
# Differential gate for the CPU MoE block switch (P1-12 integration, EXP667).
#
# Two arms of the same binaries:
#   goose   the switch active, the delivered object linked
#   native  FG_GOOSE_CPU_SIMD_NATIVE=1, the same binary taking the native arm
#
# Compares against ggml_vec_dot_*_generic, the scalar definition, bit for bit.
# Also proves the multi-block refusal and the call counter.
#
# Usage: bash integration/p1-12/tests/run_switch.sh [iterations]
set -euo pipefail

ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)
PKG=${FG_SIMD_PACKAGE:-/home/end0/AI/Fried-Goose/build/qualified/cpu-simd-8b7a56bb835bc87c}
ITERS=${1:-256}
W=$ROOT/builds/.p112-switch
CC=${CC:-gcc-13}
CXX=${CXX:-g++-13}
LL=$ROOT/upstream/llama.cpp
INC="-I $LL/ggml/src -I $LL/ggml/src/ggml-cpu -I $LL/ggml/include -I $ROOT/integration/p1-12/src"

mkdir -p "$W"

sha256sum -c --status <(printf '%s  %s\n' \
    67f7f760dd9fcfb6b9f828777ea06750f3b9a032b3b9ffe4ebd8307b055eab4e \
    "$PKG/cpu/production/blocks.o") || {
    echo "blocks.o does not match the accepted delivery" >&2; exit 1; }

build_c() { # <out> <src> <extra...>
    local out=$1 src=$2; shift 2
    [ -f "$out" ] && [ "$out" -nt "$src" ] && return 0
    $CC -O1 -std=c11 -w $INC "$@" -c "$src" -o "$out"
}

echo "[1/3] objects"
build_c "$W/ggml-quants.o"  "$LL/ggml/src/ggml-quants.c"
build_c "$W/cpu-quants.o"   "$LL/ggml/src/ggml-cpu/quants.c"
build_c "$W/native_stubs.o" "$ROOT/integration/p1-12/tests/native_stubs.c"
$CXX -O1 -std=c++17 -w $INC -c "$ROOT/integration/p1-12/src/fg-goose-cpu-simd.cpp" -o "$W/bridge.o"
$CXX -O1 -std=c++17 -w $INC -c "$ROOT/integration/p1-12/tests/switch_gate.cpp" -o "$W/gate.o"

echo "[2/3] link (the delivered object is the only Goose kernel source)"
$CXX -O1 "$W/gate.o" "$W/bridge.o" "$PKG/cpu/production/blocks.o" \
    "$W/ggml-quants.o" "$W/cpu-quants.o" "$W/native_stubs.o" \
    -lm -pthread -ldl -o "$W/switch_gate"

echo "[3/3] run both arms"
rc=0
echo -n "  goose  : "; "$W/switch_gate" "$ITERS" || rc=1
echo -n "  native : "; FG_GOOSE_CPU_SIMD_NATIVE=1 "$W/switch_gate" "$ITERS" || rc=1
exit $rc
