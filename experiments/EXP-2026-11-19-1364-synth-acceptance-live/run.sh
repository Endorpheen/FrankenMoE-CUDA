#!/usr/bin/env bash
# The synthetic acceptance decision on the real model: no draft model needed, one comparison per accepted
# position, the port's answer against the native one.
set -uo pipefail
BIN=${1:-$PWD/builds/flint-acc/bin}
OUT=${2:-/tmp/synth}
rm -rf "$OUT"; mkdir -p "$OUT"
export LD_LIBRARY_PATH="$BIN"
export FG_ACCEPT_DIAG=1
MODEL=/home/end0/AI/LocalAI/Models/Apex-35B-V1.0-Q4_K_M.gguf
setsid "$BIN/llama-server" -m "$MODEL" --alias Apex-35B-V1.0 -c 8192 -ngl all -fit off --cpu-moe \
  -fa on -ctk q4_0 -ctv q4_0 -b 2048 -ub 2048 -t 8 -tb 16 --poll 0 -np 1 \
  --moe-cache-mib 4992 --host 127.0.0.1 --port 9101 \
  --spec-type ngram-simple --spec-synth-rates 0.950,0.931,0.912,0.893,0.874,0.855,0.836,0.817,0.798,0.779,0.760,0.741,0.722,0.703,0.684,0.665,0.646,0.627,0.608,0.589,0.570,0.551,0.532,0.513,0.494,0.475,0.456,0.437,0.418,0.399,0.380,0.361,0.342,0.323,0.304,0.285,0.266,0.247,0.228,0.209,0.190,0.171,0.152,0.133,0.114,0.095,0.076,0.057 --temp 0.7 --top-p 0.95 > "$OUT/server.log" 2>&1 &
PID=$!
trap 'kill -TERM -$PID 2>/dev/null' EXIT
for i in $(seq 1 140); do
  curl -sf http://127.0.0.1:9101/health >/dev/null 2>&1 && break
  kill -0 $PID 2>/dev/null || { echo "server died"; exit 1; }
  sleep 5
done
curl -sf http://127.0.0.1:9101/health >/dev/null || { echo "not ready"; exit 1; }
# ngram-simple proposes only where the history repeats, so the prompt repeats: a prose prompt yields no drafts
# and the synthetic counter stays at zero (measured: that is exactly what happened first).
REP="The quick brown fox jumps over the lazy dog. The quick brown fox jumps over the lazy dog. The quick brown fox jumps over the lazy dog. The quick brown fox jumps over the lazy dog. Continue this text by repeating the same sentence ten times."
for q in "$REP" "$REP"; do
  curl -s -X POST -H 'Content-Type: application/json' --data "{\"messages\":[{\"role\":\"user\",\"content\":\"$q\"}],\"max_tokens\":128,\"temperature\":0.7}" \
    http://127.0.0.1:9101/v1/chat/completions > "$OUT/resp.json"
  python3 -c "
import json
d=json.load(open('$OUT/resp.json'))
c=d['choices'][0]
print('  tokens:', d.get('usage',{}).get('completion_tokens'), 'finish:', c.get('finish_reason'))" 2>/dev/null
done
echo "--- counters ---"
grep "FG_SP_SYNTH" "$OUT/server.log" | tail -3
grep -c "FG_SP_SYNTH_DIFF" "$OUT/server.log"
