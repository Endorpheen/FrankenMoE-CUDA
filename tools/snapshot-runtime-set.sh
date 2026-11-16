#!/usr/bin/env bash
# Freeze the exact library and object set that produced a measurement, with hashes.
#
# Without this a rollback restores files, not a runtime: build.py links whatever objects exist, and the objects are
# not tracked. A snapshot is the only reference that can later be compared against or restored.
#
# Usage: bash tools/snapshot-runtime-set.sh <label>
#   writes results/runtime-snapshots/<label>/ with the library, the extra objects, and MANIFEST.txt
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
W=${BUILD:-/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108/runtime/reconstructed/build}
if [ "${1:-}" = "--verify" ]; then
    # Compare the runtime in the build tree against a snapshot. Exit 1 on any difference, so "this is the set that
    # produced that number" is a checkable claim rather than a recollection.
    W=${BUILD:-/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108/runtime/reconstructed/build}
    SNAP="$ROOT/results/runtime-snapshots/${2:?usage: snapshot-runtime-set.sh --verify <label>}"
    [ -d "$SNAP" ] || { echo "no snapshot: $SNAP"; exit 1; }
    diffs=0
    ref=$(grep -m1 'libllama.so$' "$SNAP/MANIFEST.txt" | cut -d' ' -f1 || true)
    cur=$(sha256sum "$W/libllama-exp371.so.0.7.0" 2>/dev/null | cut -d' ' -f1)
    if [ "$ref" = "$cur" ]; then echo "libllama        identical"; else echo "libllama        DIFFERS"; diffs=$((diffs+1)); fi
    for f in "$SNAP"/objects/*; do
        b=$(basename "$f")
        a=$(sha256sum "$f" | cut -d' ' -f1)
        c=$(sha256sum "$W/$b" 2>/dev/null | cut -d' ' -f1)
        if [ "$a" = "$c" ]; then echo "$(printf '%-16s' "$b")identical"; else echo "$(printf '%-16s' "$b")DIFFERS"; diffs=$((diffs+1)); fi
    done
    echo "differences: $diffs"
    [ $diffs -eq 0 ] || exit 1
    exit 0
fi

LABEL=${1:?usage: snapshot-runtime-set.sh <label> | --verify <label>}
DEST="$ROOT/results/runtime-snapshots/$LABEL"
[ -e "$DEST" ] && { echo "refusing to overwrite $DEST"; exit 1; }
mkdir -p "$DEST/objects"

LIB=$W/libllama-exp371.so.0.7.0
[ -f "$LIB" ] || { echo "library not found: $LIB"; exit 1; }
cp "$LIB" "$DEST/libllama.so"

OBJS="b291.o fx286pic.o fx262a.o fx263-pic.o gates.o slru-policy.o slru-runtime.o slru-shim.o
      fg-ar.o fg-ar-bridge.o fg-q35.o fg-qg.o fg-q35-bridge.o fg-plan-ffi.o fg-gr.o fg-gr-bridge.o"
for o in $OBJS; do
    [ -f "$W/$o" ] && cp "$W/$o" "$DEST/objects/$o" || echo "absent: $o" >> "$DEST/absent.txt"
done

{
    echo "label: $LABEL"
    echo "date: $(date -Is)"
    echo "git: $(git -C "$ROOT" rev-parse HEAD 2>/dev/null || echo unknown)"
    echo "src-tree: $(git -C "$ROOT" status --porcelain runtime/reconstructed/src | wc -l) modified file(s)"
    echo "cc: $(gcc-13 --version | head -1)"
    echo
    echo "== library =="
    sha256sum "$DEST/libllama.so"
    ls -l "$DEST/libllama.so" | awk '{print "size:", $5}'
    echo
    echo "== objects =="
    ( cd "$DEST/objects" && sha256sum * 2>/dev/null )
} > "$DEST/MANIFEST.txt"

echo "snapshot written: $DEST"
sed -n '1,6p' "$DEST/MANIFEST.txt"
