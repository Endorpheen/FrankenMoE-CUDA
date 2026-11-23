#!/usr/bin/env bash
# One prefill measurement per configuration: fresh server, one 32768-token prompt,
# one predicted token, prompt_ms printed. Variants are extra ENV=VAL pairs.
# usage: prefill-sweep.sh <tag> <binary> <variant...>
set -u
P=/home/end0/.codex/worktrees/pure-goose-inkling-mini/FrankenMoE-CUDA
D="$P/build/exp1786-split-planes"
ROOT=/home/end0/AI/claude-apex-100
TAG="$1"; BINX="$2"; shift 2
RUN="$ROOT/runs/sw-$TAG-$(date +%H%M%S)"; mkdir -p "$RUN"
exec 9>/tmp/apex-test.lock; flock 9
ss -H -ltn 'sport = :9101' | grep -q . && { echo "port busy"; exit 2; }
python3 - "$D/request.json" "$RUN/req.json" <<'PY'
import json, sys
d = json.load(open(sys.argv[1])); d['n_predict'] = 1; d.pop('n_probs', None)
json.dump(d, open(sys.argv[2], 'w'))
PY
mapfile -t opts < "$D/goose-options.txt"
mapfile -t args < "$D/argv.txt"
printf '%s\n' "${opts[@]}" "$@" > "$RUN/env.txt"
setsid env "${opts[@]}" "$@" "$BINX" "${args[@]}" > "$RUN/server.log" 2>&1 &
pid=$!; echo "$pid" > "$RUN/pid.txt"
for i in $(seq 1 240); do
    kill -0 "$pid" 2>/dev/null || { echo "died"; tail -3 "$RUN/server.log"; exit 3; }
    curl -fsS --max-time 2 http://127.0.0.1:9101/health > /dev/null 2>&1 && break
    sleep 1
done
curl -fsS --max-time 300 -H 'Content-Type: application/json' --data-binary @"$RUN/req.json" \
    -o "$RUN/r1.json" -w '%{time_total}\n' http://127.0.0.1:9101/completion > "$RUN/client.txt"
curl -fsS --max-time 5 http://127.0.0.1:9101/props > "$RUN/props.json" || true
kill -TERM "$pid"
for i in $(seq 1 60); do kill -0 "$pid" 2>/dev/null || break; sleep 0.5; done
python3 - "$RUN" "$TAG" <<'PY'
import json, sys
run, tag = sys.argv[1], sys.argv[2]
t = json.load(open(run + '/r1.json'))['timings']
p = json.load(open(run + '/props.json'))
print(f"{tag:28s} prompt_ms {t['prompt_ms']:9.1f}  {t['prompt_per_second']:8.1f} tok/s  "
      f"fills {p.get('cuda_expert_fills')}  upload {p.get('cuda_expert_upload_bytes',0)/1e9:.1f} GB")
PY
