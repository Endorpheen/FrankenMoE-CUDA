#!/usr/bin/env bash
# Exercise the sequence detokeniser through its HTTP endpoint, where the server calls llama_detokenize with
# caller-owned buffers: many shapes, one model load.
set -uo pipefail
BIN=${1:-$PWD/builds/flint-acc/bin}
OUT=${2:-/tmp/seq-endpoint}
mkdir -p "$OUT"
export LD_LIBRARY_PATH="$BIN"
export FG_TK_DETOK_DIAG=1
MODEL=/home/end0/AI/LocalAI/Models/Apex-35B-V1.0-Q4_K_M.gguf
setsid "$BIN/llama-server" -m "$MODEL" --alias Apex-35B-V1.0 -c 4096 -ngl all -fit off --cpu-moe \
  -fa on -ctk q4_0 -ctv q4_0 -b 2048 -ub 2048 -t 8 -tb 16 --poll 0 -np 1 --cache-ram 0 \
  --ctx-checkpoints 2 --load-mode mmap --spec-type none --jinja --temp 1 --top-p 0.95 --top-k 20 \
  --min-p 0 --moe-cache-mib 4992 --host 127.0.0.1 --port 9101 > "$OUT/server.log" 2>&1 &
PID=$!
trap 'kill -TERM -$PID 2>/dev/null' EXIT
for i in $(seq 1 140); do
  curl -sf http://127.0.0.1:9101/health >/dev/null 2>&1 && break
  kill -0 $PID 2>/dev/null || { echo "server died"; exit 1; }
  sleep 5
done
curl -sf http://127.0.0.1:9101/health >/dev/null || { echo "not ready"; exit 1; }
fail=0
for ids in "1" "9906" "9906 1917 319" "9906 1917 319 260 12" "220 9906 220 1917"; do
  body=$(printf '{"tokens":[%s]}' "$(echo $ids | tr ' ' ',')")
  code=$(curl -s -o "$OUT/resp.json" -w '%{http_code}' -X POST -H 'Content-Type: application/json' \
         --data "$body" http://127.0.0.1:9101/detokenize)
  [ "$code" = "200" ] && printf "  ok      %-22s %s\n" "$ids" "$code" || { printf "  FAIL    %-22s %s\n" "$ids" "$code"; fail=1; }
done
echo "--- counters ---"
grep -E "FG_TK_SEQ_(PROGRESS|DIFF)" "$OUT/server.log" | tail -3
grep -c "FG_TK_SEQ_DIFF" "$OUT/server.log"
exit $fail
