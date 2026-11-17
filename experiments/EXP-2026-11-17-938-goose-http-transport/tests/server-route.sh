#!/usr/bin/env bash
# Route gate: the Goose listener plus the Goose router answer real HTTP paths.
set -euo pipefail
HERE=$(cd "$(dirname "$0")" && pwd)
ROOT=$(cd "$HERE/../../.." && pwd)
GOOSE=/home/end0/AI/LlamaServer/toolchain/goose-2026.3-string/goose
OUT=${1:-/tmp/goose-http-route}
mkdir -p "$OUT"

python3 "$ROOT/tools/compose-goose.py" "$OUT/unit.goose" \
    "$ROOT/goose/server/http-listener.goose" "$ROOT/goose/server/route-table.goose" >/dev/null
( cd "$OUT" && "$GOOSE" --standalone -o unit.c --header unit.h unit.goose >/dev/null )
gcc-13 -O1 -I"$OUT" -o "$OUT/server" "$ROOT/integration/goose-standalone/tests/http_server.c" \
    "$OUT/unit.c" "$ROOT/experiments/EXP-2026-11-17-938-goose-http-transport/source/forwards.c" -lm

PORT=0
for p in 18801 18802 18803 18804; do
    ss -H -ltn 2>/dev/null | grep -q ":$p " || { PORT=$p; break; }
done
[ "$PORT" != 0 ] || { echo "no free port"; exit 1; }

"$OUT/server" "$PORT" 4 > "$OUT/server.log" 2>&1 &
SRV=$!
sleep 1

fail=0
check() {  # path expected_status
    local got
    got=$(printf "GET %s HTTP/1.1\r\nHost: x\r\nConnection: close\r\n\r\n" "$1" \
          | timeout 5 nc 127.0.0.1 "$PORT" | head -1 | tr -d '\r')
    echo "$1 -> $got"
    case "$got" in *" $2 "*) ;; *) fail=1 ;; esac
}
check /health  200
check /props   200
check /metrics 200
check /nope    404

kill $SRV 2>/dev/null || true
wait $SRV 2>/dev/null || true
grep '^REQ' "$OUT/server.log" || true
[ "$fail" = 0 ] && echo "HTTP_ROUTES OK" || { echo "HTTP_ROUTES FAIL"; exit 1; }
