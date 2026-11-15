#!/bin/bash
# Assemble the M1 staged tree from tracked repository material only.
#
# Before this script the M1 tree existed only under /tmp: the carrier patches,
# the bridges, the host adapter and the gates were inside an experiment copy and
# could not be reproduced from a clean checkout. This script rebuilds that tree
# from three tracked inputs and nothing else:
#
#   upstream/llama.cpp   the CPU llama.cpp the carriers patch (working copy)
#   integration/m1       the bridges, the host adapter, the gates and the project
#   goose/               the canonical Goose modules the unit is composed from
#
# The composed unit (goose-gen/main.c) is generated here with the repository's
# own build script, so it is reproducible and never committed.
#
# Usage: bash tools/stage-m1-tree.sh <out-dir> [--with-unit]
#   <out-dir>    empty or absent directory; the script refuses to overwrite
#   --with-unit  compose and generate goose-gen/ (needs `goose` and gcc-13)
#
# No model, no server, no GPU. Build only.
set -euo pipefail

ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)

# Arguments.
#   <out-dir> [--with-unit]   assemble a tree
#   --check-patches           dry-run every carrier against upstream, touch nothing
CHECK_PATCHES=0
WITH_UNIT=0
OUT=""
for arg in "$@"; do
    case "$arg" in
        --check-patches) CHECK_PATCHES=1 ;;
        --with-unit)     WITH_UNIT=1 ;;
        -*)              echo "stage-m1-tree: unknown option $arg" >&2; exit 2 ;;
        *)               OUT=$arg ;;
    esac
done
if [ "$CHECK_PATCHES" -eq 0 ] && [ -z "$OUT" ]; then
    echo "usage: stage-m1-tree.sh <out-dir> [--with-unit] | --check-patches" >&2
    exit 2
fi

UPSTREAM=$ROOT/upstream/llama.cpp
INTEGRATION=$ROOT/integration/m1
GOOSE=${GOOSE:-/home/end0/AI/LlamaServer/toolchain/goose-2026.3-string/goose}
CC=${CC:-gcc-13}

for d in "$UPSTREAM" "$INTEGRATION"; do
    [ -d "$d" ] || { echo "stage-m1-tree: missing input $d" >&2; exit 1; }
done
[ -f "$UPSTREAM/CMakeLists.txt" ] || {
    echo "stage-m1-tree: $UPSTREAM is not a llama.cpp tree" >&2; exit 1; }

# --check-patches: dry-run every carrier against upstream and report, without
# creating a tree. This is the cheap pre-flight rule 13 asks for: a malformed
# patch is caught here, before an expensive staging run.
if [ "$CHECK_PATCHES" -eq 1 ]; then
    bad=0
    total=0
    while IFS=$'\t' read -r patch_file target_rel _lines; do
        case "$patch_file" in ''|\#*) continue ;; esac
        src="$INTEGRATION/patches/$patch_file"
        [ -f "$src" ] || { echo "  MISSING patch $patch_file" >&2; bad=$((bad+1)); continue; }
        total=$((total + 1))
        # Carrier patches carry paths relative to the repository layout
        # (llama.cpp/ggml/..., llama.cpp/src/...), and upstream/ is the directory
        # that contains llama.cpp/ - so upstream/ is the dry-run root for them.
        # Runtime patches carry runtime/... paths and are rooted at the repository.
        case "$target_rel" in
            llama.cpp/*) dir="$ROOT/upstream" ;;
            *)           dir="$ROOT" ;;
        esac
        if ( cd "$dir" && patch -p1 --dry-run --silent < "$src" >/dev/null 2>&1 ); then
            printf '  ok    %s\n' "$patch_file"
        else
            printf '  FAIL  %s\n' "$patch_file"
            ( cd "$dir" && patch -p1 --dry-run < "$src" 2>&1 | head -4 | sed 's/^/        /' )
            bad=$((bad+1))
        fi
    done < "$INTEGRATION/patches/TARGETS"
    echo "check-patches: $((total - bad))/$total apply, $bad failed"
    [ "$bad" -eq 0 ] || exit 1
    exit 0
fi

if [ -d "$OUT" ] && [ -n "$(ls -A "$OUT" 2>/dev/null)" ]; then
    echo "stage-m1-tree: $OUT is not empty; refusing to overwrite" >&2
    exit 1
fi
mkdir -p "$OUT"

echo "[1/6] copy upstream/llama.cpp"
# The carrier patches apply to these nine files. Copy the tree without the VCS
# metadata: the staged tree is a build input, not a repository.
tar -C "$UPSTREAM" --exclude=.git -cf - . | tar -C "$OUT/llama.cpp" -xf - 2>/dev/null || {
    mkdir -p "$OUT/llama.cpp"
    tar -C "$UPSTREAM" --exclude=.git -cf - . | tar -C "$OUT/llama.cpp" -xf -
}

echo "[2/6] copy the integration layer"
cp -r "$INTEGRATION/common" "$INTEGRATION/host" "$OUT/"
cp -r "$INTEGRATION/gates/." "$OUT/gates/"
cp "$INTEGRATION/CMakeLists.txt" "$OUT/CMakeLists.txt"
cp -r "$ROOT/runtime" "$OUT/runtime"
rm -rf "$OUT/runtime/checkpoints" "$OUT/runtime/reconstructed" "$OUT/runtime/baseline-artifacts"

echo "[3/6] apply the carrier patches"
patched=0
while IFS=$'\t' read -r patch_file target_rel _lines; do
    case "$patch_file" in ''|\#*) continue ;; esac
    src="$INTEGRATION/patches/$patch_file"
    [ -f "$src" ] || { echo "  missing patch $patch_file" >&2; exit 1; }
    target="$OUT/$target_rel"
    [ -f "$target" ] || { echo "  missing target $target_rel" >&2; exit 1; }
    ( cd "$OUT" && patch -p1 -s --forward < "$src" )
    patched=$((patched + 1))
done < "$INTEGRATION/patches/TARGETS"
echo "  applied $patched carrier patches"

echo "[4/6] place the tokenizer and GGUF carrier headers"
cp "$INTEGRATION/common/fg-goose-tokenizer-bridge.cpp" "$INTEGRATION/common/fg-goose-tokenizer-bridge.h" \
   "$OUT/llama.cpp/src/"
cp "$INTEGRATION/common/fg-goose-gguf-native.h" "$OUT/llama.cpp/ggml/src/"

mkdir -p "$OUT/goose-gen"
echo "[5/6] compose the unit"
if [ "$WITH_UNIT" -eq 1 ]; then
    [ -x "$GOOSE" ] || { echo "stage-m1-tree: no goose at $GOOSE" >&2; exit 1; }
    mkdir -p "$OUT/build"
    bash "$ROOT/tools/build-main-runtime.sh" "$OUT/build/unit" >"$OUT/build/unit.log" 2>&1
    cp "$OUT/build/unit/main.c" "$OUT/build/unit/main.h" "$OUT/goose-gen/"
    # The per-stage header shims all point at the one unit header.
    for h in config-slice gguf-stack goose-unit json-schema-grammar json \
             metrics-traces models quant-stack session spec-decide \
             stream-policy tokenizer-decisions; do
        printf '#pragma once\n// M1 shim: the merged composed unit is the only Goose unit in this tree, so a\n// per-stage generated header is a forward to it (one unit, one goose_init).\n#include "main.h"\n' \
            > "$OUT/goose-gen/$h.h"
    done
    echo "  unit: $(stat -c%s "$OUT/goose-gen/main.c") B, $(wc -l < "$OUT/goose-gen/main.h") header lines"
else
    echo "  skipped (--with-unit not given): copy a goose-gen/main.c of your own"
fi

echo "[6/6] result"
echo "  tree: $OUT"
echo "  configure: cmake -S $OUT -B $OUT/build-off -G Ninja \\"
echo "               -DCMAKE_C_COMPILER=$CC -DCMAKE_CXX_COMPILER=${CC/gcc/g++} \\"
echo "               -DFG_GOOSE_JSON_SCHEMA=OFF ... (one flag per stage)"
