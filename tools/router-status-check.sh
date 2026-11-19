#!/usr/bin/env bash
# The model status renderer, exercised through the router and compared with the native switch.
#
# The single-model Apex profile never reaches this path, so it was switched and unverified. A router started without -m, with a
# models directory holding one model, renders statuses for every listed model - which is the call site - and the diagnostic
# prints whether the ported module and the native switch agree.
set -uo pipefail
ROOT=$(cd "$(dirname "$0")/.." && pwd)
BIN=${APEX_SERVER_DIR:-$ROOT/builds/flint-acc/bin}
PORT=${PORT:-9102}
MODEL=${MODEL:-/home/end0/AI/LocalAI/Models/Apex-35B-V1.0-Q4_K_M.gguf}
OUT=$ROOT/results/router-status/$(date +%Y%m%d-%H%M%S)
mkdir -p "$OUT"
DIR=$(mktemp -d)
mkdir -p "$DIR/apex"
ln -s "$MODEL" "$DIR/apex/model.gguf"

FG_MODEL_STATUS_DIAG=1 LD_LIBRARY_PATH="$BIN" "$BIN/llama-server" --models-dir "$DIR" \
    --host 127.0.0.1 --port "$PORT" > "$OUT/server.log" 2>&1 &
PID=$!
trap 'kill $PID 2>/dev/null; wait $PID 2>/dev/null; rm -rf "$DIR"' EXIT
for i in $(seq 1 60); do
    curl -sf "http://127.0.0.1:$PORT/health" >/dev/null 2>&1 && break
    kill -0 "$PID" 2>/dev/null || { echo "FAIL: the router died"; tail -3 "$OUT/server.log"; exit 5; }
    sleep 1
done

LIST=$(curl -s "http://127.0.0.1:$PORT/models")
echo "$LIST" > "$OUT/models.json"
echo "$LIST" | head -c 200; echo
python3 - "$OUT/models.json" <<'PY'
import json, sys
d = json.load(open(sys.argv[1]))
data = d.get("data") or []
if not data:
    print("FAIL: the router listed no model, so the status renderer was never called")
    raise SystemExit(1)
status = (data[0].get("status") or {}).get("value")
print(f"router listed {len(data)} model(s), first status={status!r}")
if status != "unloaded":
    print(f"FAIL: expected 'unloaded' for a model that has not been started, got {status!r}")
    raise SystemExit(1)
PY
RC=$?
MARK=$(grep -ac "FG_MODEL_STATUS ported" "$OUT/server.log")
DIS=$(grep -ao "disagreements=[0-9]*" "$OUT/server.log" | head -1)
echo "marker lines: $MARK, $DIS"
[ "$MARK" -gt 0 ] || { echo "FAIL: the status renderer was not reached"; exit 1; }
case "$DIS" in
    *disagreements=0) ;;
    *) echo "FAIL: the ported status disagrees with the native switch ($DIS)"; exit 1 ;;
esac
[ "$RC" -eq 0 ] || exit "$RC"
echo "PASS: the ported status names the model and agrees with the native switch"
echo "artefacts: $OUT"
