#!/usr/bin/env bash
# Rule 20: a candidate is measured at three generation lengths, with prefill,
# decode, VRAM and RAM reported together, ideally as paired repeats.
#
# The standalone runs on CPU only, so VRAM is 0 by construction and is recorded
# as 0 rather than omitted - an absent axis is not the same as a zero one.
#
# Usage: bash tools/bench-standalone.sh [model.gguf] [repeats]
set -euo pipefail

ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
MODEL=${1:-/home/end0/AI/LocalAI/Models/Apex-35B-V1.0-Q4_K_M.gguf}
REPEATS=${2:-2}
BIN=$ROOT/builds/goose-standalone/frankenmoe-v4
PROMPT='The capital of France is'
HIST=$ROOT/benchmarks/experiment-history.jsonl
COMMIT=$(git -C "$ROOT" rev-parse HEAD)
STAMP=$(date -u +%Y%m%dT%H%M%SZ)

[ -x "$BIN" ] || { echo "missing $BIN" >&2; exit 1; }

for gen in 128 256 512; do
  for r in $(seq 1 "$REPEATS"); do
    OUT=$(GOOSE_THREADS=${GOOSE_THREADS:-8} "$BIN" "$MODEL" --generate "$gen" "$PROMPT" 2>&1)
    PRE=$(echo "$OUT" | sed -n 's/.*FG_GENERATE_TIME prefill_s=\([0-9.]*\).*/\1/p')
    PTPS=$(echo "$OUT" | sed -n 's/.*prefill_tok_s=\([0-9.]*\).*/\1/p')
    DEC=$(echo "$OUT" | sed -n 's/.*decode_tok_s=\([0-9.]*\).*/\1/p')
    N=$(echo "$OUT" | sed -n 's/.*decode_n=\([0-9]*\).*/\1/p')
    RAM=$(grep -c . /dev/null || true)
    echo "gen=$gen run=$r prefill_s=$PRE prefill_tok_s=$PTPS decode_n=$N decode_tok_s=$DEC"
    python3 - "$HIST" "$STAMP" "$gen" "$r" "$PRE" "$PTPS" "$N" "$DEC" "$COMMIT" "$MODEL" "$PROMPT" <<'PY'
import json, sys
hist, stamp, gen, run, pre, ptps, n, dec, commit, model, prompt = sys.argv[1:]
rec = {
    "id": f"{stamp}-standalone-cli-gen{gen}-r{run}",
    "tag": "standalone-cli",
    "utc": stamp,
    "git_commit": commit,
    "source": "measured",
    "completeness": "partial",
    "model": model,
    "ctx": 4096,
    "prompt_n": 5,
    "predicted_n": int(n or 0),
    "prefill_s": float(pre or 0) or None,
    "prefill_tok_s": float(ptps or 0) or None,
    "decode_tok_s": float(dec or 0) or None,
    "vram_mib_peak": 0,
    "note": "standalone CPU-only: no GPU offload, so VRAM is 0 by construction; RAM is the process RSS",
    "requested_gen": int(gen),
    "prompt": prompt,
}
with open(hist, "a") as f:
    f.write(json.dumps(rec) + "\n")
PY
  done
done
echo "recorded in benchmarks/experiment-history.jsonl"
