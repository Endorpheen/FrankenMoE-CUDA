#!/usr/bin/env bash
# Prefill scaling: one server, one request per prompt length, no nsys.
# Measures whether prompt_ms has a fixed per-request component beside the
# compute that scales with the prompt.
# usage: prefill-scale.sh <tag> <binary> <token-count> [token-count ...]
set -u
P=/home/end0/.codex/worktrees/pure-goose-inkling-mini/FrankenMoE-CUDA
D="$P/build/exp1786-split-planes"
TAG="$1"; BINX="$2"; shift 2
ROOT=/home/end0/AI/claude-apex-100
RUN="$ROOT/runs/$TAG-$(date +%Y%m%d-%H%M%S)"
mkdir -p "$RUN"; echo "RUN=$RUN"
exec 9>/tmp/apex-test.lock; flock 9
if ss -H -ltn 'sport = :9101' | grep -q .; then echo "port busy"; exit 2; fi
mapfile -t opts < "$D/goose-options.txt"
mapfile -t args < "$D/argv.txt"
printf '%s\n' "${opts[@]}" > "$RUN/env.txt"
sha256sum "$BINX" > "$RUN/binary.sha256"
setsid env "${opts[@]}" "$BINX" "${args[@]}" > "$RUN/server.log" 2>&1 &
pid=$!; echo "$pid" > "$RUN/pid.txt"
ok=0
for i in $(seq 1 240); do
    kill -0 "$pid" 2>/dev/null || { echo "server died"; tail -10 "$RUN/server.log"; exit 3; }
    if curl -fsS --max-time 2 http://127.0.0.1:9101/health > /dev/null 2>&1; then ok=1; break; fi
    sleep 1
done
[ "$ok" = 1 ] || { echo "no health"; kill -TERM "$pid"; exit 4; }
echo "health after ${i}s"
for n in "$@"; do
    python3 - "$D/request.json" "$RUN/req-$n.json" "$n" <<'PY'
import json, sys
src, dst, n = sys.argv[1], sys.argv[2], int(sys.argv[3])
d = json.load(open(src))
d['prompt'] = d['prompt'][:n]
d['n_predict'] = 1
d.pop('n_probs', None)
json.dump(d, open(dst, 'w'))
PY
    curl -fsS --max-time 300 -H 'Content-Type: application/json' --data-binary @"$RUN/req-$n.json" \
        -o "$RUN/r-$n.json" -w '%{time_total}\n' http://127.0.0.1:9101/completion > "$RUN/client-$n.txt"
    python3 - "$RUN/r-$n.json" "$n" <<'PY'
import json, sys
d = json.load(open(sys.argv[1])); t = d['timings']
print(f"prompt_n {t['prompt_n']:6d}  prompt_ms {t['prompt_ms']:9.1f}  {t['prompt_per_second']:8.1f} tok/s  "
      f"client {float(open(sys.argv[1].replace('r-','client-').replace('.json','.txt')).read()):.3f} s")
PY
done
curl -fsS --max-time 3 http://127.0.0.1:9101/props > "$RUN/props-after.json" || true
cat /proc/$pid/status > "$RUN/status-after.txt"
kill -TERM "$pid"
for i in $(seq 1 120); do kill -0 "$pid" 2>/dev/null || break; sleep 0.5; done
kill -0 "$pid" 2>/dev/null && echo "WARNING server alive"
ss -H -ltn 'sport = :9101' | grep -q . && echo "WARNING port held" || echo "port released"
echo "$RUN" > "$ROOT/runs/LAST-SCALE"
