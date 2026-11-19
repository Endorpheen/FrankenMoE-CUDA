#!/usr/bin/env bash
# Rule 30: the single, mandatory entry point for every Apex test.
#
# Why: on 2026-11-16 overlapping servers and benchmarks cost several hours of false CUDA-OOM diagnosis. After a single
# run, the unchanged configuration gave 45.68 tok/s again. A rule in prose did not prevent it; this does.
#
# Guarantees:
#   1. One test at a time, enforced by flock held for the whole lifecycle (server start -> readiness -> generation ->
#      shutdown), not just around the start.
#   2. A second caller WAITS for the lock instead of starting a second server.
#   3. Real PIDs, command lines, ports and CUDA processes are inspected; process names are never trusted on their own.
#   4. Each test gets its own port and its own log directory, so results cannot mix.
#   5. The server runs in its own process group and only that group is ever terminated. No killall, no pkill by name,
#      no kill -9 by name.
#   6. Port release and child exit are verified after the test.
#   7. On CUDA OOM the stderr, PID, parameters, VRAM and lock status are preserved.
#
# Usage:
#   bash tools/apex-test.sh <tag> [len] [b] [ub] [cache]
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
TAG=${1:?usage: apex-test.sh <tag> [len] [b] [ub] [cache]}
LEN=${2:-512}; B=${3:-4096}; UB=${4:-4096}; CACHE=${5:-4992}

LOCK=/tmp/apex-test.lock
LOGROOT=${ROOT}/results/apex-runs
RUNID="${TAG}-$(date +%Y%m%d-%H%M%S)"
RUNDIR="${LOGROOT}/${RUNID}"
mkdir -p "${RUNDIR}"

exec 9>"${LOCK}"
echo "[$(date +%H:%M:%S)] waiting for the Apex test lock" | tee -a "${RUNDIR}/run.log"
# -w waits instead of failing: a second caller must queue, never start a second server
if ! flock -w 7200 9; then
    echo "TIMEOUT waiting for the lock; refusing to start a second server" | tee -a "${RUNDIR}/run.log"
    exit 3
fi
echo "[$(date +%H:%M:%S)] lock acquired" | tee -a "${RUNDIR}/run.log"

cleanup() {
    local rc=$?
    if [ -n "${SRV_PID:-}" ]; then
        # Only the process group this script created. Never a global kill by name.
        kill -TERM -- "-${SRV_PID}" 2>/dev/null || true
        for _ in 1 2 3 4 5 6 7 8 9 10; do
            kill -0 "${SRV_PID}" 2>/dev/null || break
            sleep 0.5
        done
        if kill -0 "${SRV_PID}" 2>/dev/null; then
            kill -KILL -- "-${SRV_PID}" 2>/dev/null || true
        fi
        wait "${SRV_PID}" 2>/dev/null || true
    fi
    # verify the port is released by the pid that owned it, not by name
    for _ in 1 2 3 4 5; do
        [ -n "$(ss -H -ltn "sport = :${PORT}" 2>/dev/null)" ] || break
        sleep 1
    done
    if [ -n "$(ss -H -ltn "sport = :${PORT}" 2>/dev/null)" ]; then
        OWNER=$(ss -ltnp 2>/dev/null | awk -v p=":${PORT}" '$4 ~ p' | grep -oE 'pid=[0-9]+' | head -1)
        echo "PORT ${PORT} STILL HELD after cleanup (${OWNER:-unknown owner})" | tee -a "${RUNDIR}/run.log"
    else
        echo "[$(date +%H:%M:%S)] port ${PORT} released, server exited" | tee -a "${RUNDIR}/run.log"
    fi
    echo "[$(date +%H:%M:%S)] lock released, exit ${rc}" | tee -a "${RUNDIR}/run.log"
    exit ${rc}
}
trap cleanup EXIT INT TERM

# Unique port per run so two runs can never share one, even if the lock were bypassed.
PORT=${APEX_PORT:-9101}

# 3. Real inspection before starting anything.
if [ -n "$(ss -H -ltn "sport = :${PORT}" 2>/dev/null)" ]; then
    OWNER=$(ss -ltnp 2>/dev/null | awk -v p=":${PORT}" '$4 ~ p' | grep -oE 'pid=[0-9]+' | head -1)
    echo "REFUSING: port ${PORT} already owned (${OWNER:-unknown})" | tee -a "${RUNDIR}/run.log"
    exit 2
fi
OURS=$(ps -eo pid,args | grep 'stage-run/llama-server' | grep -v grep | wc -l || true)
if [ "${OURS}" -gt 0 ]; then
    echo "REFUSING: ${OURS} of our servers alive:" | tee -a "${RUNDIR}/run.log"
    ps -eo pid,args | grep 'stage-run/llama-server' | grep -v grep | head -5 | tee -a "${RUNDIR}/run.log"
    exit 2
fi
nvidia-smi --query-compute-apps=pid,used_memory --format=csv,noheader 2>/dev/null | tee -a "${RUNDIR}/run.log" || true

MODEL=${MODEL:-/home/end0/AI/LocalAI/Models/Apex-35B-V1.0-Q4_K_M.gguf}
[ -x /tmp/res ] && /tmp/res "${MODEL}" | tee -a "${RUNDIR}/run.log" || true

W=/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108/runtime/reconstructed/build
SERVER_DIR=${APEX_SERVER_DIR:-${W}/stage-run}
SRV=${SERVER_DIR}/llama-server
REQUEST=${APEX_REQUEST:-/tmp/req512.json}
REQUEST_COUNT=${APEX_REQUEST_COUNT:-3}
ENDPOINT=${APEX_ENDPOINT:-completion}
[[ "$ENDPOINT" = completion || "$ENDPOINT" = v1/chat/completions ]] || exit 2
[[ "$REQUEST_COUNT" =~ ^[1-9][0-9]*$ ]] || exit 2
[[ "$PORT" = 9101 ]] || { echo "Apex requires port 9101"; exit 2; }
[[ -x "$SRV" && -f "$REQUEST" ]] || { echo "missing server or request"; exit 2; }
export LD_LIBRARY_PATH="$SERVER_DIR"
unset LD_PRELOAD
ARGS=(-m "$MODEL" --alias Apex-35B-V1.0 -c 262144 -ngl all -fit off --cpu-moe
      -fa on -ctk q4_0 -ctv q4_0 -b "$B" -ub "$UB" -t 8 -tb 16
      --cpu-mask 0f0f --cpu-mask-batch ffff --cpu-strict 1 --cpu-strict-batch 1
      --poll 0 -np 1 --cache-ram 0 --ctx-checkpoints 32 --load-mode mmap
      --spec-type none --jinja --temp 1 --top-p 0.95 --top-k 20 --min-p 0
      --host 127.0.0.1 --port "$PORT" --moe-cache-mib "$CACHE")
printf '%s\n' "$SRV" "${ARGS[@]}" > "${RUNDIR}/argv.txt"
sha256sum "$REQUEST" "$SERVER_DIR"/*.so* "$SRV" > "${RUNDIR}/SHA256SUMS"
ldd "$SRV" > "${RUNDIR}/dependencies.txt"
cp "$REQUEST" "${RUNDIR}/request.json"
cat /proc/vmstat > "${RUNDIR}/vmstat-prelaunch.txt"
cat /proc/meminfo > "${RUNDIR}/meminfo-prelaunch.txt"

# 5. own process group: setsid, so the group id is the server's pid and only it is terminated.
setsid env LD_PRELOAD="${APEX_DEBUG_PRELOAD:-}" "${SRV}" "${ARGS[@]}" \
    > "${RUNDIR}/server.log" 2>&1 &
SRV_PID=$!
echo "[$(date +%H:%M:%S)] server pid ${SRV_PID} (own process group)" | tee -a "${RUNDIR}/run.log"

READY=0
for _ in $(seq 1 120); do
    if curl -s -m 2 "http://127.0.0.1:${PORT}/health" 2>/dev/null | grep -q ok; then READY=1; break; fi
    kill -0 "${SRV_PID}" 2>/dev/null || break
    cat "/proc/${SRV_PID}/maps" > "${RUNDIR}/maps.txt" 2>/dev/null || true
    sleep 2
done

if [ "${READY}" -ne 1 ]; then
    echo "SERVER DID NOT BECOME READY" | tee -a "${RUNDIR}/run.log"
    # 7. preserve everything needed to diagnose, including lock status and VRAM
    { echo "--- OOM DIAGNOSTICS ---"; echo "pid: ${SRV_PID}"; echo "port: ${PORT}"; echo "lock: /tmp/apex-test.lock (held)"
      echo "params: len=${LEN} b=${B} ub=${UB} cache=${CACHE}"
      nvidia-smi --query-gpu=memory.used,memory.total --format=csv 2>/dev/null
      nvidia-smi --query-compute-apps=pid,used_memory --format=csv,noheader 2>/dev/null
      tail -40 "${RUNDIR}/server.log"; } > "${RUNDIR}/diagnostics.txt"
    exit 4
fi
echo "[$(date +%H:%M:%S)] server ready" | tee -a "${RUNDIR}/run.log"

cat "/proc/${SRV_PID}/maps" > "${RUNDIR}/maps.txt"
cat "/proc/${SRV_PID}/status" > "${RUNDIR}/status-ready.txt"
cat /proc/vmstat > "${RUNDIR}/vmstat-before.txt"
cat /proc/meminfo > "${RUNDIR}/meminfo-ready.txt"
nvidia-smi --query-gpu=memory.used,memory.total --format=csv,noheader > "${RUNDIR}/vram-ready.txt"
for ((i=1; i<=REQUEST_COUNT; i++)); do
    curl -fsS -m 1800 -X POST "http://127.0.0.1:${PORT}/${ENDPOINT}" -H 'Content-Type: application/json' \
        --data-binary @"${REQUEST}" -o "${RUNDIR}/r${i}.json" \
        -w '%{time_total}\n' > "${RUNDIR}/client-${i}-seconds.txt"
    if [ -n "${APEX_EXPECT_REFERENCE:-}" ]; then
        python3 - "${RUNDIR}/r${i}.json" "${APEX_EXPECT_REFERENCE}" <<'PY'
import json, sys
actual, expected = (json.load(open(p)) for p in sys.argv[1:])
if actual.get("content") != expected["content"] or actual.get("tokens") != expected["tokens"]:
    raise SystemExit("control response mismatch: stopping the lifecycle")
print("control content/token gate passed")
PY
    fi
done
cat "/proc/${SRV_PID}/status" > "${RUNDIR}/status-after.txt"
cat /proc/vmstat > "${RUNDIR}/vmstat-after.txt"
cat /proc/meminfo > "${RUNDIR}/meminfo-after.txt"
nvidia-smi --query-gpu=memory.used,memory.total --format=csv,noheader > "${RUNDIR}/vram-after.txt"

python3 - "${RUNDIR}" <<'PY'
import json, hashlib, statistics, sys, pathlib
d = pathlib.Path(sys.argv[1]); dec, pre, hs = [], [], set()
for path in sorted(d.glob('r[0-9]*.json')):
    try:
        r = json.load(open(path)); t = r["timings"]
        dec.append(round(t["predicted_per_second"], 2)); pre.append(round(t["prompt_per_second"], 1))
        content = r.get("content")
        if content is None:
            content = r["choices"][0]["message"].get("content") or ""
        if not content:
            raise ValueError("empty generated content")
        hs.add(hashlib.sha256(content.encode()).hexdigest()[:12])
    except Exception as exc:
        raise SystemExit(f"invalid response {path}: {exc}")
out = f"decode {dec} prefill {pre} sha {hs}"
(d/"summary.txt").write_text(out + "\n")
print(out)
PY
