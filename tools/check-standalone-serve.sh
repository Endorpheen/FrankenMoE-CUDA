#!/usr/bin/env bash
# Gate: the shipping binary serves HTTP and generates through its own listener.
#
# The listener, the router, the reader, the tokenizer, the trunk and the sampler
# are all in the composed unit; this checks that the binary wires them, by talking
# to it over a socket rather than by reading symbols.
#
# Usage: bash tools/check-standalone-serve.sh [model.gguf] [outdir]
set -euo pipefail

ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
MODEL=${1:-/home/end0/AI/LocalAI/Models/Apex-35B-V1.0-Q4_K_M.gguf}
OUT=${2:-/tmp/goose-standalone-serve}
BIN=$ROOT/builds/goose-standalone/frankenmoe-v4
mkdir -p "$OUT"

[ -x "$BIN" ] || { echo "missing $BIN: build it first" >&2; exit 1; }

PORT=0
for p in 19211 19212 19213 19214; do
    ss -H -ltn 2>/dev/null | grep -q ":$p " || { PORT=$p; break; }
done
[ "$PORT" != 0 ] || { echo "no free port"; exit 1; }

GOOSE_THREADS=${GOOSE_THREADS:-8} "$BIN" "$MODEL" --serve "$PORT" 2 > "$OUT/serve.log" 2>&1 &
SRV=$!
for _ in $(seq 1 180); do grep -q 'FG_SERVE listening' "$OUT/serve.log" 2>/dev/null && break; sleep 1; done

P=$(curl -s --max-time 120 "http://127.0.0.1:$PORT/props")
C=$(curl -s --max-time 900 -X POST --data-binary 'The capital of France is' "http://127.0.0.1:$PORT/completion")
kill $SRV 2>/dev/null || true
wait $SRV 2>/dev/null || true

echo "P: $P"
echo "C: $(echo "$C" | head -c 120)..."
fail=0
echo "$P" | grep -q '"n_layer":40' || fail=1
echo "$C" | grep -q '"ids":\[11751,11,264,3177,8807,303,3712,321' || fail=1
echo "$C" | grep -q '"text":" Paris, a city rich in history' || fail=1
grep -q 'FG_SERVE_REQ' "$OUT/serve.log" || fail=1
[ "$fail" = 0 ] && echo "STANDALONE_SERVE OK" || { echo "STANDALONE_SERVE FAIL"; tail -5 "$OUT/serve.log"; exit 1; }
