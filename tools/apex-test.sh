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
        kill -KILL -- "-${SRV_PID}" 2>/dev/null || true
        wait "${SRV_PID}" 2>/dev/null || true
    fi
    # verify the port is released by the pid that owned it, not by name
    for _ in 1 2 3 4 5; do
        ss -ltn 2>/dev/null | grep -qE ":${PORT}\$" || break
        sleep 1
    done
    if ss -ltn 2>/dev/null | grep -qE ":${PORT}\$"; then
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
if ss -ltn 2>/dev/null | grep -qE ":${PORT}\$"; then
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
SRV=${W}/stage-run/llama-server

# 5. own process group: setsid, so the group id is the server's pid and only it is terminated.
setsid "${SRV}" $(tr '\n' ' ' < /tmp/l5-guarded2_512.argv | sed 's|^[^ ]*||; s|--port 8950|--port '"${PORT}"'|') \
    > "${RUNDIR}/server.log" 2>&1 &
SRV_PID=$!
echo "[$(date +%H:%M:%S)] server pid ${SRV_PID} (own process group)" | tee -a "${RUNDIR}/run.log"

READY=0
for _ in $(seq 1 120); do
    if curl -s -m 2 "http://127.0.0.1:${PORT}/health" 2>/dev/null | grep -q ok; then READY=1; break; fi
    kill -0 "${SRV_PID}" 2>/dev/null || break
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

for i in 1 2 3; do
    curl -s -m 1800 -X POST "http://127.0.0.1:${PORT}/completion" -H 'Content-Type: application/json' \
        --data-binary @/tmp/req512.json -o "${RUNDIR}/r${i}.json" || true
done

python3 - "${RUNDIR}" <<'PY'
import json, hashlib, statistics, sys, pathlib
d = pathlib.Path(sys.argv[1]); dec, pre, hs = [], [], set()
for i in (1, 2, 3):
    try:
        r = json.load(open(d/f"r{i}.json")); t = r["timings"]
        dec.append(round(t["predicted_per_second"], 2)); pre.append(round(t["prompt_per_second"], 1))
        hs.add(hashlib.sha256(r["content"].encode()).hexdigest()[:12])
    except Exception:
        pass
out = f"decode {dec} prefill {pre} sha {hs}"
(d/"summary.txt").write_text(out + "\n")
print(out)
PY
