#!/usr/bin/env bash
# Start the consolidated build, run one AWT mode against it, stop it, and pass the verdict through.
#
# The port is checked by pid before anything starts and never stolen from another process; the server is started in its
# own process group and only that group is terminated (rules 29-30). Everything the run produces - server log, requests,
# responses, summary - lands in results/awt/<tag>-<timestamp>/.
#
# usage: tools/awt/run.sh FAST|CHECK|FULL|REPRO [tag]
set -uo pipefail
R=$(cd "$(dirname "$0")/../.." && pwd)
MODE=${1:-FAST}
TAG=${2:-awt}
PORT=${AWT_PORT:-9101}
BIN=${AWT_BIN:-$R/builds/flint-acc/bin}
MODEL=${AWT_MODEL:-/home/end0/AI/LocalAI/Models/Apex-35B-V1.0-Q4_K_M.gguf}
STAMP=$(date +%Y%m%d-%H%M%S)
OUT=$R/results/awt/$TAG-$STAMP
mkdir -p "$OUT"

if ss -ltnp 2>/dev/null | grep -q ":$PORT "; then
  echo "REFUSING: port $PORT already has an owner:"
  ss -ltnp 2>/dev/null | grep ":$PORT " | sed 's/^/  /'
  echo "  stop your own server first; this script never stops another process"
  exit 2
fi
[ -x "$BIN/llama-server" ] || { echo "no server at $BIN"; exit 2; }
[ -f "$MODEL" ] || { echo "no model at $MODEL"; exit 2; }

export LD_LIBRARY_PATH="$BIN"
# The counters that prove which implementation answered are behind flags; the AWT run turns them on so the
# goose_paths check has evidence rather than an assumption.
export FG_TK_BPE_DIAG=1
export FG_TK_DETOK_DIAG=1
setsid "$BIN/llama-server" -m "$MODEL" --alias Apex-35B-V1.0 \
  -c 262144 -ngl all -fit off --cpu-moe -fa on -ctk q4_0 -ctv q4_0 \
  -b 4096 -ub 4096 -t 8 -tb 16 \
  --cpu-mask 0f0f --cpu-mask-batch ffff --cpu-strict 1 --cpu-strict-batch 1 \
  --poll 0 -np 1 --cache-ram 0 --ctx-checkpoints 32 --load-mode mmap \
  --spec-type none --jinja --temp 1 --top-p 0.95 --top-k 20 --min-p 0 \
  --moe-cache-mib 4992 --host 127.0.0.1 --port "$PORT" > "$OUT/server.log" 2>&1 &
PID=$!
cleanup() { kill -TERM -"$PID" 2>/dev/null; wait "$PID" 2>/dev/null; }
trap cleanup EXIT

for i in $(seq 1 180); do
  curl -sf "http://127.0.0.1:$PORT/health" >/dev/null 2>&1 && break
  kill -0 "$PID" 2>/dev/null || { echo "FAIL: the server died during load"; tail -5 "$OUT/server.log"; exit 1; }
  sleep 4
done
bash "$R/tools/awt/vram-trace.sh" "$OUT/startup.log" "$PID" $$ > /dev/null 2>&1 &
LOAD_TRACE=$!
curl -sf "http://127.0.0.1:$PORT/health" >/dev/null || { echo "FAIL: not ready"; exit 1; }

# a copy of the server log lives where awt.py expects it, and keeps growing as the run proceeds
# The VRAM profile of the run is sampled alongside it: a peak that grows between identical requests is a leak, and a
# constant that is too high is a configuration question - the two are separated here rather than guessed at afterwards.
nvidia-smi --query-gpu=memory.used --format=csv,noheader,nounits > "$OUT/vram_before.txt" 2>/dev/null
kill "$LOAD_TRACE" 2>/dev/null; wait "$LOAD_TRACE" 2>/dev/null
cp "$OUT/startup_vram.txt" "$OUT/vram_load.txt" 2>/dev/null
bash "$R/tools/awt/vram-trace.sh" "$OUT/server.log" "$PID" $$ > "$OUT/vram_trace.txt" 2>&1 &
TRACE=$!
python3 "$R/tools/awt/awt.py" "$MODE" --base "http://127.0.0.1:$PORT" --tag "$TAG" --repo "$R" --out "$OUT"
RC=$?
kill "$TRACE" 2>/dev/null
wait "$TRACE" 2>/dev/null
cat "$OUT/vram_trace.txt" >> "$OUT/summary.txt" 2>/dev/null
# The VRAM profile is a verdict, not a note: a peak above the recorded baseline or growth across the run fails the
# run the same way a wrong answer does.
python3 "$R/tools/awt/check-vram.py" "$OUT" | tee -a "$OUT/summary.txt"
VRAM_RC=${PIPESTATUS[0]}
if [ "$VRAM_RC" -ne 0 ]; then
    echo "FAIL: VRAM profile outside the baseline"
    RC=1
fi
if ! kill -0 "$PID" 2>/dev/null; then
  echo "FAIL: the server died during the run"
  tail -5 "$OUT/server.log"
  cat "$OUT/vram_trace.txt" 2>/dev/null
  RC=1
fi
exit $RC
