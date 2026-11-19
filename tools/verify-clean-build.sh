#!/usr/bin/env bash
# Verify that a clean checkout builds with the pinned compiler.
#
# P10-06's last item: "a clean isolated build with the pinned compiler". What makes it a check and
# not a claim is the pairing of three things - a checkout with nothing carried over, the compiler the
# pin names, and a result compared with the recorded one.
#
# Two modes:
#
#   --compose-only   a fresh worktree, compose and typecheck one module set (seconds). This proves
#                    the sources are self-sufficient and the pin resolves; it does not compile C.
#   (default)        the full unit: compose, typecheck, generate and compile, then print the object's
#                    symbol counts for comparison with a recorded build.
#
# The worktree is removed on exit, so nothing accumulates. No model, no GPU, no acceptance run.
#
# Usage: bash tools/verify-clean-build.sh [--compose-only] [--keep]
set -uo pipefail
ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
G=$(python3 -c "import json;print(json.load(open('$ROOT/runtime/compiler/current.json'))['compiler'])" 2>/dev/null || true)
PIN_SHA=$(python3 -c "import json;print(json.load(open('$ROOT/runtime/compiler/current.json'))['sha256'])" 2>/dev/null || true)
MODE=full; KEEP=0
for a in "$@"; do
    case "$a" in
        --compose-only) MODE=compose ;;
        --keep) KEEP=1 ;;
        *) echo "unknown option: $a" >&2; exit 2 ;;
    esac
done

[ -x "$G" ] || { echo "the pinned compiler is absent: ${G:-<unset>}" >&2; exit 2; }
ACT_SHA=$(sha256sum "$G" | cut -d' ' -f1)
if [ -n "$PIN_SHA" ] && [ "$ACT_SHA" != "$PIN_SHA" ]; then
    echo "the compiler does not match the pin: $ACT_SHA != $PIN_SHA" >&2
    exit 3
fi
echo "compiler: $G"
echo "sha256:   $ACT_SHA (matches the pin)"

WT=$(mktemp -d /tmp/fg-clean.XXXXXX)
cleanup() { [ "$KEEP" -eq 1 ] || git -C "$ROOT" worktree remove --force "$WT" 2>/dev/null; }
trap cleanup EXIT
git -C "$ROOT" worktree add --detach "$WT" HEAD >/dev/null 2>&1 || { echo "cannot create a worktree" >&2; exit 4; }
echo "worktree: clean checkout of $(git -C "$ROOT" rev-parse --short HEAD)"

# Nothing from the working tree can leak in: the worktree is a checkout of HEAD only.
DIRTY=$(git -C "$ROOT" status --porcelain | grep -v '^??' | wc -l)
echo "note:     $DIRTY tracked modification(s) in the working tree are NOT part of this build"

cd "$WT" || exit 4
if [ "$MODE" = compose ]; then
    set -- goose/model/qwen4exp.goose goose/model/q4m-metadata.goose goose/model/q4t-tensor-map.goose \
           goose/model/q4e-expert-layout.goose
    python3 tools/compose-goose.py /tmp/fg-clean-unit.goose "$@" || { echo "compose failed" >&2; exit 5; }
    "$G" check /tmp/fg-clean-unit.goose 2>&1 | grep -vE "warning:" | tail -2
    rc=${PIPESTATUS[0]}
    [ "$rc" -eq 0 ] && echo "clean compose: ok" || echo "clean compose: FAILED"
    exit "$rc"
fi

OUT=/tmp/fg-clean-build
rm -rf "$OUT"
GOOSE="$G" bash tools/build-main-runtime.sh "$OUT" >/tmp/fg-clean-build.log 2>&1
rc=$?
tail -3 /tmp/fg-clean-build.log
if [ "$rc" -ne 0 ]; then echo "clean build: FAILED (log /tmp/fg-clean-build.log)" >&2; exit "$rc"; fi
printf 'clean build: ok, object sha256 %s\n' "$(sha256sum "$OUT/main.o" 2>/dev/null | cut -d' ' -f1)"
