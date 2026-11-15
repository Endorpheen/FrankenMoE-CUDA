#!/bin/bash
# Independent numeric acceptance of the Fried Goose CPU SIMD block object (P1-12).
#
# The compiler owner proves its own emission against ggml_vec_dot_*_generic. This
# script proves the shipped object against the native sources of *this*
# repository: ggml-quants.c (row quantizers), ggml-cpu/quants.c (scalar kernels)
# and ggml-cpu/arch/x86/quants.c (the AVX2 kernels the shipped path runs).
#
# Both the scalar and the AVX2 arm are compared bit-for-bit. Neither is stubbed:
# the two ggml support symbols the objects need (the abort hook and the f16
# lookup table) are supplied from the real definitions in tests/native_stubs.c.
#
# Usage: bash integration/p1-12/tests/run.sh [iterations]
set -euo pipefail

ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)
PKG=${FG_SIMD_PACKAGE:-/home/end0/AI/Fried-Goose/build/qualified/cpu-simd-8b7a56bb835bc87c}
ITERS=${1:-256}
W=$ROOT/builds/.p112
CC=${CC:-gcc-13}
LL=$ROOT/upstream/llama.cpp

mkdir -p "$W"

[ -f "$PKG/cpu/production/blocks.o" ] || { echo "missing $PKG/cpu/production/blocks.o" >&2; exit 1; }
EXPECTED_OBJ=67f7f760dd9fcfb6b9f828777ea06750f3b9a032b3b9ffe4ebd8307b055eab4e
GOT_OBJ=$(sha256sum "$PKG/cpu/production/blocks.o" | cut -d' ' -f1)
[ "$GOT_OBJ" = "$EXPECTED_OBJ" ] || {
    echo "blocks.o hash $GOT_OBJ does not match the delivery $EXPECTED_OBJ" >&2; exit 1; }

INC="-I $LL/ggml/src -I $LL/ggml/src/ggml-cpu -I $LL/ggml/include"

build() { # <out> <src> <extra flags...>
    local out=$1 src=$2; shift 2
    [ -f "$out" ] && [ "$out" -nt "$src" ] && return 0
    $CC -O1 -std=c11 -w $INC "$@" -c "$src" -o "$out"
}

echo "[1/3] native objects"
build "$W/ggml-quants.o" "$LL/ggml/src/ggml-quants.c"
build "$W/cpu-quants.o"  "$LL/ggml/src/ggml-cpu/quants.c"
build "$W/x86-quants.o"  "$LL/ggml/src/ggml-cpu/arch/x86/quants.c" -mavx2 -mfma
build "$W/native_stubs.o" "$ROOT/integration/p1-12/tests/native_stubs.c"
cp -f "$PKG/cpu/production/blocks.h" "$W/fg_cpu_simd_blocks.h" || { chmod u+w "$W/fg_cpu_simd_blocks.h"; cp -f "$PKG/cpu/production/blocks.h" "$W/fg_cpu_simd_blocks.h"; }

echo "[2/3] link"
$CC -O1 -std=c11 -w -I "$W" -c "$ROOT/integration/p1-12/tests/native_parity.c" -o "$W/native_parity.o"
$CC -O1 "$W/native_parity.o" "$PKG/cpu/production/blocks.o" \
    "$W/ggml-quants.o" "$W/x86-quants.o" "$W/cpu-quants.o" "$W/native_stubs.o" \
    -lm -pthread -o "$W/native_parity"

echo "[3/3] run"
"$W/native_parity" "$ITERS"
