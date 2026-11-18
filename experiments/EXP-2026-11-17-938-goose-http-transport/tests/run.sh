#!/usr/bin/env bash
# Listener gate: bring the Goose listener up on a free port, send one real
# request, and require a well-formed 200 with the router-visible path.
set -euo pipefail
HERE=$(cd "$(dirname "$0")" && pwd)
ROOT=$(cd "$HERE/../../.." && pwd)
GOOSE=/home/end0/AI/LlamaServer/toolchain/goose-2026.3-string/goose
OUT=${1:-/tmp/goose-http-gate}
mkdir -p "$OUT"

"$GOOSE" --standalone -o "$OUT/listener.c" --header "$OUT/listener.h" "$ROOT/goose/server/http-listener.goose" >/dev/null
gcc-13 -O1 -I"$OUT" -o "$OUT/server" "$HERE/server.c" "$OUT/listener.c" "$ROOT/experiments/EXP-2026-11-17-938-goose-http-transport/source/forwards.c" -lm

PORT=0
for p in 18781 18782 18783 18784; do
    if ! ss -H -ltn 2>/dev/null | grep -q ":$p "; then PORT=$p; break; fi
done
[ "$PORT" != 0 ] || { echo "no free port"; exit 1; }

"$OUT/server" "$PORT" > "$OUT/server.log" 2>&1 &
SRV=$!
sleep 1
RESP=$(printf 'GET /props?x=1 HTTP/1.1\r\nHost: x\r\nConnection: keep-alive\r\n\r\n' | timeout 5 nc 127.0.0.1 "$PORT" || true)
kill $SRV 2>/dev/null || true
wait $SRV 2>/dev/null || true

echo "$RESP" | head -5
if echo "$RESP" | head -1 | grep -q 'HTTP/1.1 200 OK' && echo "$RESP" | grep -q '{"ok":true}'; then
    echo "HTTP_LISTENER OK"
else
    echo "HTTP_LISTENER FAIL"
    cat "$OUT/server.log"
    exit 1
fi
