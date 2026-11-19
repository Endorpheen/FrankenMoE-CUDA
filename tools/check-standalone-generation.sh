#!/usr/bin/env bash
# Gate: the shipping standalone binary generates the accepted token sequence.
#
# The reference is the EXP922 record for "The capital of France is":
#   ids 11751 11 264 3177 8807 303 3712 321
#   text " Paris, a city rich in history and"
# It was produced by the composed unit long before the server or the CLI existed,
# so it is an independent target rather than something this script wrote down.
#
# Usage: bash tools/check-standalone-generation.sh [model.gguf]
set -euo pipefail

ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
MODEL=${1:-/home/end0/AI/LocalAI/Models/Apex-35B-V1.0-Q4_K_M.gguf}
BIN=$ROOT/builds/goose-standalone/frankenmoe-v4
GOOSE_THREADS=${GOOSE_THREADS:-8}
WANT_IDS="11751 11 264 3177 8807 303 3712 321"
WANT_TEXT=' Paris, a city rich in history and'

if [ ! -x "$BIN" ]; then
    echo "missing $BIN: run bash tools/build-standalone-runtime.sh first" >&2
    exit 1
fi

OUT=$(GOOSE_THREADS=$GOOSE_THREADS "$BIN" "$MODEL" --generate 8 2>&1)
echo "$OUT" | grep -E 'FG_GENERATE' || true

# only the generation line; the binary also prints a tokenisation probe line
IDS=$(echo "$OUT" | grep '^FG_GENERATE vocab=' | sed -n 's/.*ids= *\(.*\)$/\1/p' | head -1 | sed 's/ *$//')
TEXT=$(echo "$OUT" | sed -n 's/^text="\(.*\)"$/\1/p' | head -1)

fail=0
[ "$IDS" = "$WANT_IDS" ] || { echo "ids differ: got [$IDS]"; fail=1; }
[ "$TEXT" = "$WANT_TEXT" ] || { echo "text differs: got [$TEXT]"; fail=1; }
if [ "$fail" = 0 ]; then echo "STANDALONE_GENERATION OK"; else echo "STANDALONE_GENERATION FAIL"; exit 1; fi
