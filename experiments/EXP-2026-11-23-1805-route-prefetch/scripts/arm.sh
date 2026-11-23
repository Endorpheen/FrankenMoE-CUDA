#!/usr/bin/env bash
# One Apex arm: fresh server, warm-up request, one measured request, clean stop.
# usage: arm.sh <tag> <binary> [extra ENV=VAL ...]
set -u
P=/home/end0/.codex/worktrees/pure-goose-inkling-mini/FrankenMoE-CUDA
D="$P/build/exp1786-split-planes"
TAG="$1"; BINX="$2"; shift 2
RUN="/home/end0/AI/claude-apex-100/runs/$TAG-$(date +%Y%m%d-%H%M%S)"
mkdir -p "$RUN"; echo "RUN=$RUN"
exec 9>/tmp/apex-test.lock; flock 9
if ss -H -ltn 'sport = :9101' | grep -q .; then echo "port 9101 busy"; ss -H -ltnp 'sport = :9101'; exit 2; fi
mapfile -t opts < "$D/goose-options.txt"
mapfile -t args < "$D/argv.txt"
printf '%s\n' "${opts[@]}" "$@" > "$RUN/env.txt"
printf '%s\n' "${args[@]}" > "$RUN/argv.txt"
sha256sum "$BINX" > "$RUN/binary.sha256"
nvidia-smi --query-gpu=memory.used,memory.free,temperature.gpu,clocks.gr,clocks.mem --format=csv > "$RUN/gpu-before.csv"
cat /proc/vmstat > "$RUN/vmstat-start.txt"
setsid env "${opts[@]}" "$@" "$BINX" "${args[@]}" > "$RUN/server.log" 2>&1 &
pid=$!; echo "$pid" > "$RUN/pid.txt"
for i in $(seq 1 180); do
  kill -0 "$pid" 2>/dev/null || { echo "server died"; tail -20 "$RUN/server.log"; exit 3; }
  curl -fsS --max-time 2 http://127.0.0.1:9101/health > "$RUN/health.json" 2>/dev/null && break
  sleep 1
done
echo "health after ${i}s"
[ "$(readlink -f /proc/$pid/exe)" = "$(realpath "$BINX")" ] || { echo "exe mismatch"; kill -TERM "$pid"; exit 4; }
nvidia-smi --query-compute-apps=pid,used_gpu_memory --format=csv,noheader,nounits --loop-ms=250 > "$RUN/vram.csv" &
vp=$!
for n in 1 2; do
  curl -fsS --max-time 120 -H 'Content-Type: application/json' --data-binary @"$D/request.json" \
    -w '%{time_total}\n' -o "$RUN/r$n.json" http://127.0.0.1:9101/completion > "$RUN/client-$n-seconds.txt"
  echo "request $n exit $?"
done
cat /proc/$pid/status > "$RUN/status-after.txt"
cat /proc/vmstat > "$RUN/vmstat-after.txt"
curl -fsS --max-time 3 http://127.0.0.1:9101/props > "$RUN/props-after.json" || true
kill -TERM "$vp"; wait "$vp" 2>/dev/null
kill -TERM "$pid"
for i in $(seq 1 60); do kill -0 "$pid" 2>/dev/null || break; sleep 0.5; done
kill -0 "$pid" 2>/dev/null && echo "WARNING server still alive pid=$pid"
ss -H -ltn 'sport = :9101' | grep -q . && echo "WARNING port still held" || echo "port released"
nvidia-smi --query-gpu=temperature.gpu,clocks.gr,clocks.mem --format=csv > "$RUN/gpu-after.csv"
echo "$RUN" > /home/end0/AI/claude-apex-100/runs/LAST
