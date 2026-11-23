#!/usr/bin/env bash
# Stack sampling of a running FrankenMoE server during one prefill request.
# gdb attach/detach per sample; identifies the host loop that dominates prefill.
# usage: prefill-sample.sh <binary> [samples]
set -u
P=/home/end0/.codex/worktrees/pure-goose-inkling-mini/FrankenMoE-CUDA
D="$P/build/exp1786-split-planes"
BINX="$1"; N="${2:-14}"
ROOT=/home/end0/AI/claude-apex-100
RUN="$ROOT/runs/prefill-stacks-$(date +%Y%m%d-%H%M%S)"
mkdir -p "$RUN"; echo "RUN=$RUN"
exec 9>/tmp/apex-test.lock; flock 9
ss -H -ltn 'sport = :9101' | grep -q . && { echo "port busy"; exit 2; }
mapfile -t opts < "$D/goose-options.txt"
mapfile -t args < "$D/argv.txt"
python3 - "$D/request.json" "$RUN/req.json" <<'PY'
import json, sys
d = json.load(open(sys.argv[1])); d['n_predict'] = 1; d.pop('n_probs', None)
json.dump(d, open(sys.argv[2], 'w'))
PY
setsid env "${opts[@]}" "$BINX" "${args[@]}" > "$RUN/server.log" 2>&1 &
pid=$!; echo "$pid" > "$RUN/pid.txt"
for i in $(seq 1 240); do
    curl -fsS --max-time 2 http://127.0.0.1:9101/health > /dev/null 2>&1 && break
    sleep 1
done
echo "health after ${i}s, pid $pid"
curl -fsS --max-time 200 -H 'Content-Type: application/json' --data-binary @"$RUN/req.json" \
    -o "$RUN/warm.json" http://127.0.0.1:9101/completion
curl -fsS --max-time 200 -H 'Content-Type: application/json' --data-binary @"$RUN/req.json" \
    -o "$RUN/r1.json" -w 'client %{time_total}\n' http://127.0.0.1:9101/completion &
CURL=$!
for s in $(seq 1 "$N"); do
    gdb -p "$pid" -batch -ex "thread apply all bt 6" > "$RUN/sample-$s.txt" 2>/dev/null
    sleep 0.35
done
wait $CURL 2>/dev/null
python3 - "$RUN/r1.json" <<'PY'
import json, sys
t = json.load(open(sys.argv[1]))['timings']
print(f"prompt_ms {t['prompt_ms']:.1f}  {t['prompt_per_second']:.1f} tok/s")
PY
kill -TERM "$pid"
for i in $(seq 1 60); do kill -0 "$pid" 2>/dev/null || break; sleep 0.5; done
ss -H -ltn 'sport = :9101' | grep -q . && echo "port held" || echo "port released"
python3 - "$RUN" <<'PY'
import glob, re, collections, sys
run = sys.argv[1]
top = collections.Counter(); frames = collections.Counter(); per_thread = collections.Counter()
for f in sorted(glob.glob(run + '/sample-*.txt')):
    for m in re.finditer(r'Thread \d+ \(Thread [^)]*\):\n((?:#\d+.*\n)+)', open(f).read()):
        block = m.group(1).strip().splitlines()
        names = [re.sub(r'^#\d+\s+(?:0x[0-9a-f]+ in )?', '', l).split(' (')[0].strip() for l in block]
        names = [n for n in names if n and n != '??']
        if not names: continue
        frames[names[0]] += 1
        for n in names[:4]: top[n] += 1
print('\nmost frequent innermost frames')
for n, c in frames.most_common(12): print(f"  {c:3d}  {n}")
print('\nframes anywhere in the top four')
for n, c in top.most_common(14): print(f"  {c:3d}  {n}")
PY
echo "$RUN" > "$ROOT/runs/LAST-STACKS"
