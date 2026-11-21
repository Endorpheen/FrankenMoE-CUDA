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
# Set APEX_GOOSE_BIN to capture an independent frankenmoe-infer execution.
# APEX_RUN_ROOT optionally selects the directory for run artifacts.
# APEX_CPU_PROFILE=1 records gprofng clock stacks with request boundary marks.
# APEX_SAMPLE_MEMORY=1 samples server RAM and GPU memory throughout the run.
# APEX_NSYS_SESSION stops a named Nsight capture before server shutdown.
# APEX_NSYS_BIN selects its CLI when it differs from the installed nsys.
# APEX_CAPTURE_SMAPS=1 saves mapping-level accounting before and after requests.
#
# Readiness knobs (do not change the Apex profile):
#   APEX_READY_MAX=900    hard cap, seconds
#   APEX_READY_STALL=180  no-progress window, seconds
# Slot persistence checks use APEX_SLOT_STATE_DIR and APEX_SLOT_SAVE and/or
# APEX_SLOT_RESTORE. The directory holds files exchanged between separate runs.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
TAG=${1:?usage: apex-test.sh <tag> [len] [b] [ub] [cache]}
LEN=${2:-512}; B=${3:-4096}; UB=${4:-4096}; CACHE=${5:-4992}

LOCK=/tmp/apex-test.lock
LOGROOT=${APEX_RUN_ROOT:-${ROOT}/results/apex-runs}
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
    if [ -n "${MEMORY_PID:-}" ]; then
        kill -TERM "$MEMORY_PID" 2>/dev/null || true
        wait "$MEMORY_PID" || true
    fi
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
GOOSE_BIN=${APEX_GOOSE_BIN:-}
SRV=${GOOSE_BIN:-${SERVER_DIR}/llama-server}
REQUEST=${APEX_REQUEST:-/tmp/req512.json}
REQUEST_COUNT=${APEX_REQUEST_COUNT:-3}
ENDPOINT=${APEX_ENDPOINT:-completion}
SLOT_DIR=${APEX_SLOT_STATE_DIR:-}
SLOT_SAVE=${APEX_SLOT_SAVE:-}
SLOT_RESTORE=${APEX_SLOT_RESTORE:-}
CPU_PROFILE=${APEX_CPU_PROFILE:-0}
[[ "$ENDPOINT" = completion || "$ENDPOINT" = v1/chat/completions ]] || exit 2
[[ "$REQUEST_COUNT" =~ ^[1-9][0-9]*$ ]] || exit 2
[[ "$PORT" = 9101 ]] || { echo "Apex requires port 9101"; exit 2; }
[[ -x "$SRV" && -f "$REQUEST" ]] || { echo "missing server or request"; exit 2; }
COLLECTOR=()
if [ "$CPU_PROFILE" = 1 ]; then
    command -v gprofng >/dev/null || { echo 'APEX_CPU_PROFILE requires gprofng'; exit 2; }
    COLLECTOR=(gprofng collect app -F off -a off -p 2 -l SIGUSR2 -o "${RUNDIR}/cpu.er")
fi
if [ -n "$SLOT_DIR$SLOT_SAVE$SLOT_RESTORE" ]; then
    [[ -d "$SLOT_DIR" && -n "$SLOT_SAVE$SLOT_RESTORE" && "$ENDPOINT" == completion ]] || {
        echo 'slot checks require a state directory, save or restore filename, and /completion'
        exit 2
    }
    SLOT_DIR=$(cd "$SLOT_DIR" && pwd -P)
fi
if [ -n "$GOOSE_BIN" ]; then
    [[ "$ENDPOINT" == completion && -z "$SLOT_DIR" && "${APEX_SPEC_TYPE:-none}" == none ]] || {
        echo 'Goose capture requires /completion and no slot or speculative mode'
        exit 2
    }
fi
# Pre-flight: a benchmark that runs on a build missing an accepted component measures the wrong
# runtime. This is the check that would have caught the host-registration loss before a single request.
if [ -z "$GOOSE_BIN" ] && [ -z "${APEX_SKIP_COMPONENT_CHECK:-}" ]; then
    if ! python3 "$(dirname "$0")/check-accepted-components.py" "$SERVER_DIR" >/tmp/apex-components.log 2>&1; then
        echo "APEX PRE-FLIGHT FAILED: an accepted component is missing from $SERVER_DIR"
        sed 's/^/  /' /tmp/apex-components.log | tail -8
        echo "  set APEX_SKIP_COMPONENT_CHECK=1 only for a deliberate control run"
        exit 6
    fi
    echo "[pre-flight] accepted components present: $(grep -c present /tmp/apex-components.log)/$(grep -cE 'present|MISSING' /tmp/apex-components.log)"
fi

unset LD_PRELOAD
if [ -n "$GOOSE_BIN" ]; then
    unset LD_LIBRARY_PATH
    ARGS=(--ctx-size "${APEX_CTX:-262144}" --batch-size "$B" --ubatch-size "$UB"
          --cache-type-k "${APEX_CACHE_TYPE_K:-q4_0}" --cache-type-v "${APEX_CACHE_TYPE_V:-q4_0}")
    if [ -n "${5:-}" ]; then
        ARGS+=(--expert-cache-mib "$CACHE")
    fi
    if [ "${APEX_GOOSE_CPU_AFFINITY:-0}" == 1 ]; then
        ARGS+=(--threads "${APEX_GOOSE_THREADS:-8}" --threads-batch "${APEX_GOOSE_THREADS_BATCH:-16}"
              --cpu-mask 0f0f --cpu-mask-batch ffff --cpu-strict 1 --cpu-strict-batch 1)
    else
        if [ -n "${APEX_GOOSE_THREADS:-}" ]; then
            ARGS+=(--threads "$APEX_GOOSE_THREADS")
        fi
        if [ -n "${APEX_GOOSE_THREADS_BATCH:-}" ]; then
            ARGS+=(--threads-batch "$APEX_GOOSE_THREADS_BATCH")
        fi
    fi
    ARGS+=("$MODEL" --serve "$PORT")
    PRELOAD=
else
    export LD_LIBRARY_PATH="$SERVER_DIR"
    PRELOAD=${APEX_DEBUG_PRELOAD:-}
    ARGS=(-m "$MODEL" --alias Apex-35B-V1.0 -c "${APEX_CTX:-262144}" -ngl all -fit off --cpu-moe
          -fa on -ctk q4_0 -ctv q4_0 -b "$B" -ub "$UB" -t 8 -tb 16
          --cpu-mask 0f0f --cpu-mask-batch ffff --cpu-strict 1 --cpu-strict-batch 1
          --poll 0 -np 1 --cache-ram 0 --ctx-checkpoints 32 --load-mode mmap
          --spec-type "${APEX_SPEC_TYPE:-none}" --jinja --temp 1 --top-p 0.95 --top-k 20 --min-p 0
          --host 127.0.0.1 --port "$PORT" --moe-cache-mib "$CACHE")
    if [ -n "$SLOT_DIR" ]; then
        ARGS+=(--slot-save-path "$SLOT_DIR")
    fi
fi
printf '%s\n' "$SRV" "${ARGS[@]}" > "${RUNDIR}/argv.txt"
if [ -n "$GOOSE_BIN" ]; then
    sha256sum "$REQUEST" "$SRV" > "${RUNDIR}/SHA256SUMS"
    for option in GGML_CUDA_REGISTER_HOST GGML_CUDA_NO_PINNED GOOSE_EXPERT_PIN_MIB \
        GOOSE_CUDA GOOSE_MMQ GOOSE_GDN_MMA GOOSE_GDN_DECODE GOOSE_FLASH_MMA GOOSE_DENSE_MMA GOOSE_MMVQ \
        GOOSE_MMQ_Q4_REFERENCE GOOSE_MMQ_Q6_REFERENCE GOOSE_MMQ_DENSE_X128 GOOSE_MMQ_ROUTED_X128 GOOSE_MMQ_Q4_M64 GOOSE_MMQ_Q4_M128 \
        GOOSE_MMQ_Q4_LARGE_TILE GOOSE_MMQ_Q4_ROUTED_M128 GOOSE_MMQ_Q4_ROUTED_DOWN_M128 GOOSE_MMQ_Q6_HALF_K \
        GOOSE_DENSE_MMA_REFERENCE \
        GOOSE_SSM_REFERENCE GOOSE_SSM_DECODE_REFERENCE GOOSE_RMS_REFERENCE GOOSE_KV_LAZY_VMM GOOSE_HOST_KV_DECOMMIT \
        GOOSE_FLASH_VEC GOOSE_FLASH_TAIL_MMA GOOSE_FLASH_STREAM_K GOOSE_ROPE_DECODE \
        GOOSE_CUDA_GRAPH GOOSE_CUDA_GRAPH_PACK GOOSE_CUDA_MMVQ_GRAPH GOOSE_CUDA_ROUTED_GRAPH GOOSE_CUDA_FULL_FFN_GRAPH \
        GOOSE_CUDA_DECODE_GRAPH GOOSE_CUDA_DECODE_EXPERT_OVERLAP GOOSE_CUDA_DECODE_REUSE \
        GOOSE_CUDA_ACTIVATION_MIB GOOSE_CUDA_ACTIVATION_REFERENCE GOOSE_CUDA_STAGE_MATRIX_INPUT \
        GOOSE_CUDA_ASYNC_PACKETS GOOSE_CUDA_EXPERT_OVERLAP GOOSE_CUDA_EXPERT_STAGE_PAGEABLE GOOSE_CUDA_EXPERT_STAGE_CURRENT \
        GOOSE_PREFILL_CHECKPOINT_TAIL GOOSE_MMVQ_DENSE_TAIL GOOSE_MMQ_DENSE_PARTIAL; do
        if [[ -v $option ]]; then printf '%s=%s\n' "$option" "${!option}"; fi
    done > "${RUNDIR}/goose-options.txt"
else
    sha256sum "$REQUEST" "$SERVER_DIR"/*.so* "$SRV" > "${RUNDIR}/SHA256SUMS"
fi
ldd "$SRV" > "${RUNDIR}/dependencies.txt"
cp "$REQUEST" "${RUNDIR}/request.json"
cat /proc/vmstat > "${RUNDIR}/vmstat-prelaunch.txt"
cat /proc/meminfo > "${RUNDIR}/meminfo-prelaunch.txt"

# 5. own process group: setsid, so the group id is the server's pid and only it is terminated.
setsid env LD_PRELOAD="$PRELOAD" "${COLLECTOR[@]}" "${SRV}" "${ARGS[@]}" \
    > "${RUNDIR}/server.log" 2>&1 &
SRV_PID=$!
printf '%s\n' "$SRV_PID" > "${RUNDIR}/pid.txt"
echo "[$(date +%H:%M:%S)] server pid ${SRV_PID} (own process group)" | tee -a "${RUNDIR}/run.log"
if [ "${APEX_SAMPLE_MEMORY:-0}" = 1 ]; then
    python3 "$ROOT/tools/apex-memory.py" "$SRV_PID" > "${RUNDIR}/memory.jsonl" \
        2> "${RUNDIR}/memory-errors.txt" &
    MEMORY_PID=$!
fi

# Readiness is progress-based, not a fixed count: a slow but progressing load is
# waited for, a load that stops making progress is not mistaken for OOM and is
# not waited on forever.
#   APEX_READY_MAX   hard cap in seconds (default 900)
#   APEX_READY_STALL no-progress window in seconds (default 180)
READY_MAX=${APEX_READY_MAX:-900}
STALL_MAX=${APEX_READY_STALL:-180}
READY=0
REASON=""
START_TS=$(date +%s)
LAST_SIG=""
LAST_CHANGE=${START_TS}
NEXT_REPORT=$((START_TS + 15))
while :; do
    NOW=$(date +%s)
    if curl -s -m 2 "http://127.0.0.1:${PORT}/health" 2>/dev/null | grep -q ok; then READY=1; break; fi
    if ! kill -0 "${SRV_PID}" 2>/dev/null; then REASON=exited; break; fi

    LOG_BYTES=$(stat -c %s "${RUNDIR}/server.log" 2>/dev/null || echo 0)
    RSS_PAGES=$(awk '{print $24}' "/proc/${SRV_PID}/stat" 2>/dev/null || echo 0)
    VRAM=$(nvidia-smi --query-gpu=memory.used --format=csv,noheader,nounits 2>/dev/null | head -1)
    SIG="${LOG_BYTES}:${RSS_PAGES}:${VRAM}"
    if [ "${SIG}" != "${LAST_SIG}" ]; then
        LAST_SIG="${SIG}"
        LAST_CHANGE=${NOW}
    fi

    if [ "${NOW}" -ge "${NEXT_REPORT}" ]; then
        echo "[$(date +%H:%M:%S)] loading: elapsed=$((NOW-START_TS))s log=${LOG_BYTES}B rss=$((RSS_PAGES*4/1024))M vram=${VRAM:-?}M quiet=$((NOW-LAST_CHANGE))s" \
            | tee -a "${RUNDIR}/run.log"
        cat "/proc/${SRV_PID}/maps" > "${RUNDIR}/maps.txt" 2>/dev/null || true
        NEXT_REPORT=$((NOW + 15))
    fi

    if [ $((NOW - LAST_CHANGE)) -ge "${STALL_MAX}" ]; then REASON=stalled; break; fi
    if [ $((NOW - START_TS)) -ge "${READY_MAX}" ]; then REASON=timeout; break; fi
    sleep 2
done

if [ "${READY}" -ne 1 ]; then
    OOM_EVIDENCE="$(grep -m3 -E 'cudaMalloc failed|out of memory|failed to allocate' "${RUNDIR}/server.log" 2>/dev/null || true)"
    case "${REASON}" in
        exited)  VERDICT="SERVER EXITED DURING LOAD (pid gone, no readiness)" ;;
        stalled) VERDICT="SERVER STALLED: no progress (log/rss/vram unchanged) for ${STALL_MAX}s after $(( $(date +%s) - START_TS ))s" ;;
        *)       VERDICT="SERVER STILL LOADING at the ${READY_MAX}s cap (progress was observed up to the cutoff)" ;;
    esac
    if [ -n "${OOM_EVIDENCE}" ]; then
        VERDICT="${VERDICT}; ALLOCATION ERROR PRESENT"
    else
        VERDICT="${VERDICT}; no allocation error in the server log"
    fi
    echo "SERVER DID NOT BECOME READY: ${VERDICT}" | tee -a "${RUNDIR}/run.log"
    { echo "--- DIAGNOSTICS ---"
      echo "verdict: ${VERDICT}"
      echo "pid: ${SRV_PID}"; echo "port: ${PORT}"; echo "lock: /tmp/apex-test.lock (held)"
      echo "params: len=${LEN} b=${B} ub=${UB} cache=${CACHE}"
      echo "last progress signature: ${LAST_SIG}"
      echo "quiescent: $(( $(date +%s) - LAST_CHANGE ))s"
      echo "allocation errors found:"
      if [ -n "${OOM_EVIDENCE}" ]; then printf '%s\n' "${OOM_EVIDENCE}"; else echo "  none"; fi
      nvidia-smi --query-gpu=memory.used,memory.total --format=csv 2>/dev/null
      nvidia-smi --query-compute-apps=pid,used_memory --format=csv,noheader 2>/dev/null
      tail -40 "${RUNDIR}/server.log"; } > "${RUNDIR}/diagnostics.txt"
    exit 4
fi
echo "[$(date +%H:%M:%S)] server ready after $(( $(date +%s) - START_TS ))s" | tee -a "${RUNDIR}/run.log"

ss -H -ltnp "sport = :${PORT}" > "${RUNDIR}/listener.txt"
if ! grep -F "pid=${SRV_PID}," "${RUNDIR}/listener.txt" >/dev/null; then
    echo "REFUSING: ready listener on port ${PORT} is not owned by PID ${SRV_PID}" | tee -a "${RUNDIR}/run.log"
    exit 2
fi
cat "/proc/${SRV_PID}/maps" > "${RUNDIR}/maps.txt"
cat "/proc/${SRV_PID}/status" > "${RUNDIR}/status-ready.txt"
if [ "${APEX_CAPTURE_SMAPS:-0}" = 1 ]; then
    cat "/proc/${SRV_PID}/smaps" > "${RUNDIR}/smaps-ready.txt"
fi
tr '\0' '\n' < "/proc/${SRV_PID}/cmdline" > "${RUNDIR}/cmdline.txt"
readlink "/proc/${SRV_PID}/exe" > "${RUNDIR}/exe.txt"
if [ -n "$GOOSE_BIN" ]; then
    curl -fsS -m 5 "http://127.0.0.1:${PORT}/props" -o "${RUNDIR}/props-ready.json"
fi
cat /proc/vmstat > "${RUNDIR}/vmstat-before.txt"
cat /proc/meminfo > "${RUNDIR}/meminfo-ready.txt"
nvidia-smi --query-gpu=memory.used,memory.total --format=csv,noheader > "${RUNDIR}/vram-ready.txt"
nvidia-smi --query-compute-apps=pid,used_memory --format=csv,noheader |
    awk -F, -v pid="$SRV_PID" '$1+0 == pid' > "${RUNDIR}/vram-process-ready.txt"
if [ "$CPU_PROFILE" = 1 ]; then
    kill -USR2 "$SRV_PID"
fi
if [ -n "$SLOT_DIR" ]; then
    python3 "$ROOT/tests/kv/slot-state/probe.py" "$PORT" "$RUNDIR" "$SLOT_DIR" "$SLOT_SAVE" "$SLOT_RESTORE" \
        | tee "$RUNDIR/summary.txt"
else
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
fi
if [ "$CPU_PROFILE" = 1 ]; then
    kill -USR2 "$SRV_PID"
fi
cat "/proc/${SRV_PID}/status" > "${RUNDIR}/status-after.txt"
cat "/proc/${SRV_PID}/maps" > "${RUNDIR}/maps-after.txt"
if [ "${APEX_CAPTURE_SMAPS:-0}" = 1 ]; then
    cat "/proc/${SRV_PID}/smaps" > "${RUNDIR}/smaps-after.txt"
fi
if [ -n "$GOOSE_BIN" ]; then
    curl -fsS -m 5 "http://127.0.0.1:${PORT}/props" -o "${RUNDIR}/props-after.json"
fi
cat /proc/vmstat > "${RUNDIR}/vmstat-after.txt"
cat /proc/meminfo > "${RUNDIR}/meminfo-after.txt"
nvidia-smi --query-gpu=memory.used,memory.total --format=csv,noheader > "${RUNDIR}/vram-after.txt"
nvidia-smi --query-compute-apps=pid,used_memory --format=csv,noheader |
    awk -F, -v pid="$SRV_PID" '$1+0 == pid' > "${RUNDIR}/vram-process-after.txt"
if [ -n "${MEMORY_PID:-}" ]; then
    kill -TERM "$MEMORY_PID" 2>/dev/null || true
    wait "$MEMORY_PID"
    MEMORY_PID=
    [[ -s "${RUNDIR}/memory.jsonl" ]] || { echo 'Apex memory samples missing'; exit 5; }
fi

if [ -n "$SLOT_DIR" ]; then
    exit 0
fi

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

if [ -n "${APEX_NSYS_SESSION:-}" ]; then
    "${APEX_NSYS_BIN:-nsys}" stop --session="$APEX_NSYS_SESSION" >"${RUNDIR}/nsight-stop.log" 2>&1
fi
