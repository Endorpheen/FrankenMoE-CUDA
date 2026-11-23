#!/usr/bin/env bash
# One Apex prefill profile: fresh server under nsys, one prefill-dominant request,
# clean stop, sqlite export and budget analysis.
#
# The existing arm-nsys.sh captures only the apex-decode NVTX range, which is why
# every saved prefill trace starts at the first decode step. This one captures the
# whole process and bounds the prefill window by the first MMQ kernel and the first
# decode read-back, so no NVTX range is needed.
#
# usage: prefill-profile.sh <tag> <binary> [extra ENV=VAL ...]
set -u
P=/home/end0/.codex/worktrees/pure-goose-inkling-mini/FrankenMoE-CUDA
D="$P/build/exp1786-split-planes"
TAG="$1"; BINX="$2"; shift 2
ROOT=/home/end0/AI/claude-apex-100
RUN="$ROOT/runs/$TAG-$(date +%Y%m%d-%H%M%S)"
mkdir -p "$RUN"; echo "RUN=$RUN"
exec 9>/tmp/apex-test.lock; flock 9
if ss -H -ltn 'sport = :9101' | grep -q .; then echo "port 9101 busy"; ss -H -ltnp 'sport = :9101'; exit 2; fi
for e in /proc/[0-9]*/exe; do
    t=$(readlink -f "$e" 2>/dev/null) || continue
    case "$t" in *frankenmoe*) echo "frankenmoe already running: ${e#/proc/} $t"; exit 2;; esac
done
# Prefill-only request: the same 32768-token prompt as the acceptance request,
# one predicted token, so the trace is dominated by the prompt pass.
python3 - "$D/request.json" "$RUN/request-prefill.json" <<'PY'
import json, sys
src, dst = sys.argv[1], sys.argv[2]
d = json.load(open(src))
d['n_predict'] = 1
d.pop('n_probs', None)
json.dump(d, open(dst, 'w'))
print('prefill request: prompt', len(d['prompt']), 'n_predict', d['n_predict'])
PY
mapfile -t opts < "$D/goose-options.txt"
mapfile -t args < "$D/argv.txt"
printf '%s\n' "${opts[@]}" "$@" > "$RUN/env.txt"
printf '%s\n' "${args[@]}" > "$RUN/argv.txt"
sha256sum "$BINX" > "$RUN/binary.sha256"
cat /proc/vmstat > "$RUN/vmstat-start.txt"
nvidia-smi --query-gpu=memory.used,memory.free,temperature.gpu,clocks.gr,clocks.mem --format=csv > "$RUN/gpu-before.csv"
setsid env "${opts[@]}" "$@" /usr/local/bin/nsys profile --trace=cuda --sample=none --cpuctxsw=none \
    --cuda-graph-trace=node --force-overwrite=true -o "$RUN/prof" "$BINX" "${args[@]}" \
    > "$RUN/server.log" 2>&1 &
pid=$!; echo "$pid" > "$RUN/pid.txt"
ok=0
for i in $(seq 1 240); do
    kill -0 "$pid" 2>/dev/null || { echo "server died"; tail -20 "$RUN/server.log"; exit 3; }
    if curl -fsS --max-time 2 http://127.0.0.1:9101/health > "$RUN/health.json" 2>/dev/null; then ok=1; break; fi
    sleep 1
done
[ "$ok" = 1 ] || { echo "no health"; tail -20 "$RUN/server.log"; kill -TERM "$pid"; exit 4; }
echo "health after ${i}s"
curl -fsS --max-time 300 -H 'Content-Type: application/json' --data-binary @"$RUN/request-prefill.json" \
    -w '%{time_total}\n' -o "$RUN/r1.json" http://127.0.0.1:9101/completion > "$RUN/client-1-seconds.txt"
echo "request exit $?"
curl -fsS --max-time 3 http://127.0.0.1:9101/props > "$RUN/props-after.json" || true
cat /proc/$pid/status > "$RUN/status-after.txt"
cat /proc/vmstat > "$RUN/vmstat-after.txt"
kill -TERM "$pid"
for i in $(seq 1 120); do kill -0 "$pid" 2>/dev/null || break; sleep 0.5; done
if kill -0 "$pid" 2>/dev/null; then echo "WARNING server still alive pid=$pid"; fi
for i in $(seq 1 120); do [ -s "$RUN/prof.nsys-rep" ] && break; sleep 1; done
ls -la "$RUN/prof.nsys-rep" 2>/dev/null | awk '{print "report bytes:", $5}'
nsys export --type sqlite --force-overwrite true -o "$RUN/prof.sqlite" "$RUN/prof.nsys-rep" > "$RUN/export.log" 2>&1
[ -s "$RUN/prof.sqlite" ] && echo "sqlite exported" || { echo "export failed"; tail -5 "$RUN/export.log"; exit 5; }
ss -H -ltn 'sport = :9101' | grep -q . && echo "WARNING port still held" || echo "port released"
python3 "$ROOT/repo/tools/prefill-budget.py" "$RUN/prof.sqlite" | tee "$RUN/budget.txt"
echo "$RUN" > "$ROOT/runs/LAST"
