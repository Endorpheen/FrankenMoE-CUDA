#!/usr/bin/env bash
# The HTTP surface of the shipped server: every endpoint a client depends on, asked directly.
# Functional check; the profile is the baseline profile with a small context so the load is short.
set -uo pipefail
BIN=${1:-$PWD/builds/flint-acc/bin}
OUT=${2:-/tmp/server-surface}
mkdir -p "$OUT"
export LD_LIBRARY_PATH="$BIN"
MODEL=/home/end0/AI/LocalAI/Models/Apex-35B-V1.0-Q4_K_M.gguf
setsid "$BIN/llama-server" -m "$MODEL" --alias Apex-35B-V1.0 -c 8192 -ngl all -fit off --cpu-moe \
  -fa on -ctk q4_0 -ctv q4_0 -b 4096 -ub 4096 -t 8 -tb 16 \
  --cpu-mask 0f0f --cpu-mask-batch ffff --cpu-strict 1 --cpu-strict-batch 1 \
  --poll 0 -np 1 --cache-ram 0 --ctx-checkpoints 4 --load-mode mmap --spec-type none --jinja \
  --temp 1 --top-p 0.95 --top-k 20 --min-p 0 --moe-cache-mib 4992 --host 127.0.0.1 --port 9101 \
  > "$OUT/server.log" 2>&1 &
PID=$!
trap 'kill -TERM -$PID 2>/dev/null' EXIT
for i in $(seq 1 140); do
  curl -sf http://127.0.0.1:9101/health >/dev/null 2>&1 && break
  kill -0 $PID 2>/dev/null || { echo "server died"; tail -5 "$OUT/server.log"; exit 1; }
  sleep 5
done
curl -sf http://127.0.0.1:9101/health >/dev/null || { echo "not ready"; exit 1; }

fail=0
check() { # name path expect-substring [method] [header] [body]
    local name=$1 path=$2 want=${3:-} method=${4:-GET} header=${5:-} body=${6:-}
    local args=(-s -X "$method" -o "$OUT/$name.body" -w '%{http_code}')
    [ -n "$header" ] && args+=(-H "$header")
    [ -n "$body" ] && args+=(--data "$body")
    local code
    code=$(curl "${args[@]}" "http://127.0.0.1:9101$path")
    local ok=1
    [ "$code" = "200" ] || ok=0
    [ -n "$want" ] && { grep -q "$want" "$OUT/$name.body" || ok=0; }
    if [ $ok -eq 1 ]; then printf "  ok      %-10s %s\n" "$name" "$code"
    else printf "  FAIL    %-10s %s\n" "$name" "$code"; fail=1; fi
}
check health   /health   ""                                        GET
check props    /props    '"model_path"'                            GET
check models   /v1/models 'Apex-35B-V1.0'                          GET
check root     /         '<'                                       GET "Accept-Encoding: gzip"
check tokenize /tokenize 'tokens'                                  POST "Content-Type: application/json" '{"content":"hello"}'

exit $fail
