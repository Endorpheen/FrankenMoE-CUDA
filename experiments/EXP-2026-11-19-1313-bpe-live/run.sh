#!/usr/bin/env bash
# Live BPE cross-check: the ported tokenizer against the shipped one, on real text, counted by the
# binary's own diagnostic (FG_TK_DIAG). Functional check; the profile is the baseline profile.
set -uo pipefail
BIN=${1:-$PWD/builds/flint-acc/bin}
OUT=${2:-/tmp/bpe-live}
mkdir -p "$OUT"
export LD_LIBRARY_PATH="$BIN"
export FG_TK_DIAG=1
MODEL=/home/end0/AI/LocalAI/Models/Apex-35B-V1.0-Q4_K_M.gguf
setsid "$BIN/llama-server" -m "$MODEL" --alias Apex-35B-V1.0 -c 16384 -ngl all -fit off --cpu-moe \
  -fa on -ctk q4_0 -ctv q4_0 -b 4096 -ub 4096 -t 8 -tb 16 \
  --cpu-mask 0f0f --cpu-mask-batch ffff --cpu-strict 1 --cpu-strict-batch 1 \
  --poll 0 -np 1 --cache-ram 0 --ctx-checkpoints 4 --load-mode mmap --spec-type none --jinja \
  --temp 1 --top-p 0.95 --top-k 20 --min-p 0 --moe-cache-mib 4992 --host 127.0.0.1 --port 9101 \
  > "$OUT/server.log" 2>&1 &
PID=$!
trap 'kill -TERM -$PID 2>/dev/null' EXIT
for i in $(seq 1 140); do
  curl -sf http://127.0.0.1:9101/health >/dev/null 2>&1 && break
  kill -0 $PID 2>/dev/null || { echo "server died"; tail -6 "$OUT/server.log"; exit 1; }
  sleep 5
done
curl -sf http://127.0.0.1:9101/health >/dev/null || { echo "not ready"; exit 1; }
PROMPT=$(python3 - <<'PY'
import json
# a prompt with mixed scripts, digits, punctuation and repeated code-like fragments: the cases a BPE
# comparator can get wrong are rare characters and long words that split across merges
text = ("The quick brown fox jumps over the lazy dog. "
        "JSON: {\"k\": [1,2,3], \"s\": \"v\"}; snake_case, camelCase, kebab-case. "
        "Тест кириллицы и 日本語のテキスト, Ελληνικά, عربى. "
        "Numbers 1234567890 and code `x = y + z;` and email a.b@example.com. ") * 6
print(json.dumps({"prompt": text, "n_predict": 32, "seed": 123, "temperature": 0.0}))
PY
)
echo "$PROMPT" > "$OUT/req.json"
curl -s http://127.0.0.1:9101/completion -H 'Content-Type: application/json' --data-binary @"$OUT/req.json" -o "$OUT/response.json"
grep -oE "FG_TK_BPE_(PROGRESS|TOTAL) calls=[0-9]+ disagreements=[0-9]+" "$OUT/server.log" | tail -3
echo "content chars: $(python3 -c "import json;print(len(json.load(open('$OUT/response.json')).get('content','')))")"
