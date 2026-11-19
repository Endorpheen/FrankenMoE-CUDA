#!/usr/bin/env bash
# Permanent check: the model status and source renderers, exercised through the router.
#
# The single-model Apex profile never reaches this path, so it was switched and unverified. A
# router started without -m over a models directory renders statuses for every listed model -
# that is the call site - and the diagnostic prints whether the ported module and the native
# switch agree. This is the only check that exercises the real router listing for P8-06.
#
# Tier: model. It starts a server and loads a router, so it never runs implicitly and needs:
#   - FG_ALLOW_MODEL_RUN=1, the explicit permission AGENTS.md rule 1 requires before any server or
#     model starts. Without it the test skips (exit 77) and does nothing at all;
#   - the model file (MODEL, default below),
#   - the built server,
#   - the fixed endpoint 127.0.0.1:9101 free of any other owner (AGENTS.md rules 21 and 29).
# Any of those absent means a skip (exit 77), never a silent pass.
#
# Usage: bash tests/server/router-status/run.sh
set -uo pipefail
D="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$D/../../.." && pwd)"
BIN=${APEX_SERVER_DIR:-$ROOT/builds/flint-acc/bin}
PORT=9101
MODEL=${MODEL:-/home/end0/AI/LocalAI/Models/Apex-35B-V1.0-Q4_K_M.gguf}
STAMP=$(date +%Y%m%d-%H%M%S)
OUT=$D/build/$STAMP
mkdir -p "$OUT"

if [ "${FG_ALLOW_MODEL_RUN:-0}" != "1" ]; then
    printf 'skip: set FG_ALLOW_MODEL_RUN=1 to allow starting a server and loading the model\n' \
        | tee "$OUT/run.log"
    exit 77
fi
if [ ! -x "$BIN/llama-server" ]; then
    printf 'skip: %s/llama-server is not built\n' "${BIN#$ROOT/}" | tee "$OUT/run.log"
    exit 77
fi
if [ ! -f "$MODEL" ]; then
    printf 'skip: model %s is absent\n' "$MODEL" | tee "$OUT/run.log"
    exit 77
fi
# Rule 29: the owner of the port decides, by pid, never by process name.
if ss -ltnp 2>/dev/null | grep -q ":$PORT "; then
    printf 'skip: port %s already has an owner:\n' "$PORT" | tee "$OUT/run.log"
    ss -ltnp 2>/dev/null | grep ":$PORT " | tee -a "$OUT/run.log"
    exit 77
fi

DIR=$(mktemp -d)
mkdir -p "$DIR/apex"
ln -s "$MODEL" "$DIR/apex/model.gguf"
PID=""
cleanup() {
    if [ -n "$PID" ]; then
        kill "$PID" 2>/dev/null
        for _ in $(seq 1 20); do kill -0 "$PID" 2>/dev/null || break; sleep 0.5; done
        kill -9 "$PID" 2>/dev/null
        wait "$PID" 2>/dev/null
        # the port must be released by the pid that owned it, not assumed to be
        ss -ltnp 2>/dev/null | grep -q ":$PORT " && echo "WARNING: port $PORT is still owned after cleanup"
    fi
    rm -rf "$DIR"
}
trap cleanup EXIT

{
    printf 'library: %s\n' "${BIN#$ROOT/}"
    printf 'sha256:  %s\n' "$(sha256sum "$BIN/libllama.so.0" 2>/dev/null | cut -d' ' -f1)"
    printf 'source:  %s\n' "$(git -C "$ROOT" rev-parse --short HEAD 2>/dev/null || echo unknown)"
    printf 'model:   %s\n' "$MODEL"
    printf '$ llama-server --models-dir <tmp> --host 127.0.0.1 --port %s\n' "$PORT"

    # No setsid here: $! must be the server's own pid, or the cleanup kills the wrong process and
    # leaves a server holding the port (observed on the first run of this script).
    FG_MODEL_STATUS_DIAG=1 LD_LIBRARY_PATH="$BIN" "$BIN/llama-server" --models-dir "$DIR" \
        --host 127.0.0.1 --port "$PORT" > "$OUT/server.log" 2>&1 &
    PID=$!

    healthy=0
    for _ in $(seq 1 60); do
        if curl -sf "http://127.0.0.1:$PORT/health" >/dev/null 2>&1; then healthy=1; break; fi
        kill -0 "$PID" 2>/dev/null || { echo "FAIL: the router died"; tail -3 "$OUT/server.log"; exit 5; }
        sleep 1
    done
    [ "$healthy" -eq 1 ] || { echo "FAIL: the router never became healthy"; tail -3 "$OUT/server.log"; exit 5; }

    curl -s "http://127.0.0.1:$PORT/models" > "$OUT/models.json"
    head -c 200 "$OUT/models.json"; echo

    python3 - "$OUT/models.json" <<'PY'
import json, sys
d = json.load(open(sys.argv[1]))
data = d.get("data") or []
if not data:
    print("FAIL: the router listed no model, so the status renderer was never called")
    raise SystemExit(1)
status = (data[0].get("status") or {}).get("value")
source = data[0].get("source")
print(f"router listed {len(data)} model(s), first status={status!r} source={source!r}")
if status != "unloaded":
    print(f"FAIL: expected 'unloaded' for a model that has not been started, got {status!r}")
    raise SystemExit(1)
if source != "models_dir":
    print(f"FAIL: expected source 'models_dir' for a model listed from a directory, got {source!r}")
    raise SystemExit(1)
PY
    rc=$?

    mark=$(grep -ac "FG_MODEL_STATUS ported" "$OUT/server.log")
    dis=$(grep -ao "disagreements=[0-9]*" "$OUT/server.log" | head -1)
    echo "marker lines: $mark, $dis"
    [ "$mark" -gt 0 ] || { echo "FAIL: the status renderer was not reached"; exit 1; }
    src=$(grep -ao "FG_MODEL_SOURCE ported calls=[0-9]* disagreements=[0-9]*" "$OUT/server.log" | head -1)
    echo "FG_MODEL_SOURCE: ${src:-absent}"
    [ -n "$src" ] || { echo "FAIL: FG_MODEL_SOURCE was never called on this path"; exit 1; }
    case "$src" in
        *disagreements=0) ;;
        *) echo "FAIL: FG_MODEL_SOURCE disagrees with the native switch"; exit 1 ;;
    esac
    case "$dis" in
        *disagreements=0) ;;
        *) echo "FAIL: the ported status disagrees with the native switch (${dis:-absent})"; exit 1 ;;
    esac
    [ "$rc" -eq 0 ] || exit "$rc"
    echo "PASS: the ported status names the model and agrees with the native switch"
    echo "artefacts: ${OUT#$ROOT/}"
} 2>&1 | tee "$OUT/run.log"
rc=${PIPESTATUS[0]}
printf '[exit %d]\n' "$rc" | tee -a "$OUT/run.log"
exit "$rc"
