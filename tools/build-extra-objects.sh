#!/usr/bin/env bash
# Regenerate every extra object that runtime/reconstructed/build.py links into libllama.
#
# build.py links whatever exists, so the binary depends on directory state rather than on the repository. This
# script is the missing half: one recorded command per object, identical every time, so an object can be rebuilt
# and compared instead of trusted.
#
# Usage: bash tools/build-extra-objects.sh [--check]
#   --check  rebuild into a temporary directory and compare with what is in the build tree; report differences
#            without touching the linked objects.
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
GOOSE=${GOOSE:-/home/end0/AI/LlamaServer/toolchain/goose-2026.3-string/goose}
FLINT=${FLINT:-/home/end0/AI/LlamaServer/llama.cpp-flint-cuda}
W=${BUILD:-/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108/runtime/reconstructed/build}
CHECK=0; [ "${1:-}" = "--check" ] && CHECK=1
OUT=$(mktemp -d /tmp/xobj.XXXXXX); trap 'rm -rf "$OUT"' EXIT
DEST=$W; [ $CHECK -eq 1 ] && DEST=$OUT

CC_C="gcc-13 -O2 -fPIC -std=c11 -w"
CC_G="gcc-13 -O2 -fPIC -std=c11 -w -I${W}"

say() { printf '%-18s %s\n' "$1" "$2"; }

# --- objects generated from Goose modules -------------------------------------------------------------
gen() {  # gen <module.goose> <prefix> <out.o> <defines...>
    local mod=$1 pre=$2 out=$3; shift 3
    "$GOOSE" -o "$OUT/$pre.c" --header "$OUT/$pre.h" "$ROOT/$mod" >/dev/null
    gcc-13 -O2 -fPIC -std=c11 -w "$@" -c "$OUT/$pre.c" -o "$DEST/$out"
}

gen goose/model/arch-registry.goose ar fg-ar.o -Dgoose_init=fg_arch_registry_goose_init
gen goose/model/qwen35moe.goose  q35 fg-q35.o -Dgoose_init=fg_q35_goose_init
gen goose/model/qgate-split.goose qg fg-qg.o  -Dgoose_init=fg_qg_goose_init
gen goose/cpu/graph-reuse.goose   gr fg-gr.o  -Dgoose_init=fg_graph_reuse_goose_init

# --- bridges (plain C, no code generation) ------------------------------------------------------------
$CC_C -c "$ROOT/integration/m1/common/fg-arch-registry-bridge.c" -o "$DEST/fg-ar-bridge.o"
$CC_C -c "$ROOT/integration/m1/common/fg-apex-plan-bridge.c"    -o "$DEST/fg-q35-bridge.o"
$CC_C -c "$ROOT/integration/m1/common/fg-graph-reuse-bridge.c"  -o "$DEST/fg-gr-bridge.o"
# The five FFI functions qwen35moe/qgate-split need beyond their own extern lists, extracted from the standalone
# bindings so this object carries nothing else.
$CC_C -I"$FLINT/include" -I"$FLINT/ggml/include" -c "$ROOT/integration/m1/common/fg-apex-plan-ffi.c" -o "$DEST/fg-plan-ffi.o"

# --- report -------------------------------------------------------------------------------------------
if [ $CHECK -eq 1 ]; then
    diffs=0
    for f in fg-ar.o fg-ar-bridge.o fg-q35.o fg-qg.o fg-q35-bridge.o fg-plan-ffi.o fg-gr.o fg-gr-bridge.o; do
        a=$(sha256sum "$W/$f" 2>/dev/null | cut -d' ' -f1)
        b=$(sha256sum "$OUT/$f" 2>/dev/null | cut -d' ' -f1)
        if [ "$a" = "$b" ]; then say "$f" "identical"; else say "$f" "DIFFERS (built ${a:0:12} vs current ${b:0:12})"; diffs=$((diffs+1)); fi
    done
    echo "extra objects differing: $diffs of 8"
else
    echo "rebuilt 8 extra objects into $W"
fi
