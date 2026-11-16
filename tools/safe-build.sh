#!/usr/bin/env bash
# Build, but never at the cost of the build that is running now.
#
# The rule this enforces: before anything overwrites the working library, its exact bytes and objects are saved.
# Earlier in this project the reverse happened - the library that produced the best numbers was rebuilt over 
# dozens of times and no copy survived, so "go back to what worked" had no answer. This makes that answer exist.
#
# Usage:
#   bash tools/safe-build.sh tree  <tree-dir> [args...]    build from a consolidated tree
#   bash tools/safe-build.sh py    [args...]               build with runtime/reconstructed/build.py
#
# Exit codes from the underlying build are passed through unchanged.
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
MODE=${1:?usage: safe-build.sh tree <tree-dir> | safe-build.sh py}
shift

LABEL=$(date +%Y%m%d-%H%M%S)
SNAP="results/runtime-snapshots/pre-build-$LABEL"

echo "== saving the current runtime before building =="
if bash "$ROOT/tools/snapshot-runtime-set.sh" "pre-build-$LABEL" 2>/dev/null; then
    echo "   saved to $SNAP"
else
    echo "   WARNING: snapshot did not complete; the previous build is not protected for this run"
fi

echo "== building =="
case "$MODE" in
    tree)
        TREE=${1:?usage: safe-build.sh tree <tree-dir>}; shift
        TREE="$TREE" bash "$ROOT/tools/build-from-tree.sh" "$@"
        ;;
    py)
        python3 "$ROOT/runtime/reconstructed/build.py" "$@"
        ;;
    *)
        echo "unknown mode: $MODE"; exit 2
        ;;
esac

echo "== result =="
LIB=$(ls -t /home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108/runtime/reconstructed/build/libllama-exp371.so.0.7.0 2>/dev/null | head -1)
[ -n "$LIB" ] && echo "   libllama-exp371.so.0.7.0  $(stat -c%s "$LIB") bytes  $(sha256sum "$LIB" | cut -c1-16)"
echo "   to go back: extract $(basename "$SNAP") from the runtime-store, or rebuild from the tree recorded there"
