#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${BUILD_DIR:-${ROOT_DIR}/build/franken-cuda}"
MODEL_PATH="${1:-${MODEL_PATH:-}}"
if [[ -z "${MODEL_PATH}" ]]; then
    echo "Usage: $0 /path/to/Qwen...-00001-of-00003.gguf" >&2
    exit 2
fi

for part in 00001 00002 00003; do
    shard="${MODEL_PATH/00001-of-00003/${part}-of-00003}"
    [[ -s "${shard}" ]] || { echo "Missing shard: ${shard}" >&2; exit 2; }
done

MEM_AVAILABLE_KIB="$(awk '/MemAvailable:/ {print $2}' /proc/meminfo)"
if (( MEM_AVAILABLE_KIB < 8 * 1024 * 1024 )); then
    echo "Insufficient MemAvailable: at least 8 GiB of headroom is required" >&2
    exit 3
fi

VRAM_REQUEST_MB="${VRAM_CACHE_MB:-6144}"
VRAM_RESERVE_MB="${VRAM_RESERVE_MB:-4096}"
VRAM_FREE_MB="$(nvidia-smi --query-gpu=memory.free --format=csv,noheader,nounits | head -1 | tr -d ' ')"
VRAM_SAFE_MB=$((VRAM_FREE_MB - VRAM_RESERVE_MB))
if (( VRAM_SAFE_MB <= 0 )); then
    echo "Insufficient free VRAM: ${VRAM_FREE_MB} MiB with a ${VRAM_RESERVE_MB} MiB reserve" >&2
    exit 3
fi
if (( VRAM_REQUEST_MB > VRAM_SAFE_MB )); then
    echo "Reducing VRAM hot cache from ${VRAM_REQUEST_MB} to ${VRAM_SAFE_MB} MiB to preserve headroom" >&2
    VRAM_REQUEST_MB="${VRAM_SAFE_MB}"
fi

mkdir -p "${ROOT_DIR}/results"
CSV_PATH="${CSV_PATH:-${ROOT_DIR}/results/qwen38-latest.csv}"
MONITOR_PATH="${MONITOR_PATH:-${ROOT_DIR}/results/qwen38-monitor.csv}"
JSON_PATH="${JSON_PATH:-${ROOT_DIR}/results/qwen38-latest.json}"

echo "timestamp,rss_kib,process_swap_kib,gpu_used_mib,gpu_temp_c,gpu_util_pct,gpu_mem_util_pct,read_bytes,process_cpu_ticks,system_pswpin,system_pswpout" >"${MONITOR_PATH}"
BASE_GPU="$(nvidia-smi --query-gpu=memory.used,temperature.gpu,utilization.gpu,utilization.memory --format=csv,noheader,nounits | head -1 | tr -d ' ')"
BASE_SWAP_IN="$(awk '$1 == "pswpin" {print $2}' /proc/vmstat)"
BASE_SWAP_OUT="$(awk '$1 == "pswpout" {print $2}' /proc/vmstat)"
printf '%(%s)T,0,0,%s,0,0,%s,%s\n' -1 "${BASE_GPU:-0,0,0,0}" "${BASE_SWAP_IN}" "${BASE_SWAP_OUT}" >>"${MONITOR_PATH}"

# The generic --chatml template makes qwen4exp emit an immediate end-of-generation token.
# A raw prompt produces coherent text until architecture-specific chat-template support lands.
"${BUILD_DIR}/cli/bmoe-cli" \
    -m "${MODEL_PATH}" \
    -p "${PROMPT:-Briefly explain why addressed expert loading is useful for MoE models.}" \
    -n "${N_PREDICT:-128}" -c "${CTX_SIZE:-4096}" --ubatch "${UBATCH:-256}" \
    -t "${THREADS:-16}" \
    --gpu-layers -1 --vram-cache-mb "${VRAM_REQUEST_MB}" \
    --moe-stream --cache-mb auto --cache-floor-mb "${RAM_FLOOR_MB:-8192}" \
    --cache-ceil-mb "${RAM_CACHE_CEIL_MB:-32768}" --dense-weights anon \
    --row-stream --row-stream-mb "${ROW_CACHE_MB:-64}" \
    --io-threads "${IO_THREADS:-4}" --overlap \
    --csv "${CSV_PATH}" &
RUN_PID=$!

# Deliver a visible, escalating shutdown sequence: INT, then TERM, then KILL.
stop_run() {
    local sig="${1:-INT}"
    if [[ -n "${RUN_PID:-}" ]] && kill -0 "${RUN_PID}" 2>/dev/null; then
        if kill "-${sig}" "${RUN_PID}" 2>>"${MONITOR_PATH%.csv}-stop.log"; then
            echo "stop_run: delivered SIG${sig} to process ${RUN_PID}" >>"${MONITOR_PATH%.csv}-stop.log"
        else
            echo "stop_run: failed to deliver SIG${sig} to process ${RUN_PID}" >>"${MONITOR_PATH%.csv}-stop.log"
        fi
    fi
}

stop_children() {
    stop_run INT
    sleep 5
    stop_run TERM
    sleep 3
    if [[ -n "${RUN_PID:-}" ]] && kill -0 "${RUN_PID}" 2>/dev/null; then
        echo "stop_run: escalating to SIGKILL" >>"${MONITOR_PATH%.csv}-stop.log"
        kill -9 "${RUN_PID}" 2>/dev/null || true
    fi
    if [[ -n "${MONITOR_PID:-}" ]]; then
        kill "${MONITOR_PID}" 2>/dev/null || true
    fi
}
trap stop_children INT TERM

(
    last_in="$(awk '$1 == "pswpin" {print $2}' /proc/vmstat)"
    last_out="$(awk '$1 == "pswpout" {print $2}' /proc/vmstat)"
    last_proc_swap=0
    burst_secs=0
    while kill -0 "${RUN_PID}" 2>/dev/null; do
        rss="$(awk '$1 == "VmRSS:" {print $2}' "/proc/${RUN_PID}/status" 2>/dev/null || echo 0)"
        proc_swap="$(awk '$1 == "VmSwap:" {print $2}' "/proc/${RUN_PID}/status" 2>/dev/null || echo 0)"
        read_bytes="$(awk '$1 == "read_bytes:" {print $2}' "/proc/${RUN_PID}/io" 2>/dev/null || echo 0)"
        gpu="$(nvidia-smi --query-gpu=memory.used,temperature.gpu,utilization.gpu,utilization.memory --format=csv,noheader,nounits | head -1 | tr -d ' ')"
        cpu_ticks="$(awk '{print $14 + $15}' "/proc/${RUN_PID}/stat" 2>/dev/null || echo 0)"
        cur_in="$(awk '$1 == "pswpin" {print $2}' /proc/vmstat)"
        cur_out="$(awk '$1 == "pswpout" {print $2}' /proc/vmstat)"
        printf '%(%s)T,%s,%s,%s,%s,%s,%s,%s,%s\n' -1 "${rss:-0}" "${proc_swap:-0}" "${gpu:-0,0,0,0}" \
            "${read_bytes:-0}" "${cpu_ticks:-0}" "${cur_in:-0}" "${cur_out:-0}" >>"${MONITOR_PATH}"

        # Process VmSwap is a direct danger signal. System swap counters may include unrelated
        # activity, so require either a catastrophic spike or several sustained intervals.
        proc_burst=$(( proc_swap - last_proc_swap ))
        if (( proc_burst > 32768 )); then
            echo "Process swap grew by $(( proc_burst * 4 / 1024 )) MiB in one second; stopping" >&2
            stop_children
            break
        fi
        sys_rate=$(( (cur_in - last_in + cur_out - last_out) * 4 / 1024 ))
        # Keep the increment inside the condition: an unconditional command group previously made
        # the guard inherit the wrong exit status and stop on rates below the threshold.
        if (( sys_rate > 1048576 )) || { (( sys_rate > 128 )) && (( ++burst_secs >= 3 )); }; then
            echo "Sustained system swap I/O at ${sys_rate} MiB/s for ${burst_secs}s; stopping" >&2
            stop_children
            break
        fi
        (( sys_rate > 128 )) || burst_secs=0
        last_in="${cur_in}"
        last_out="${cur_out}"
        last_proc_swap="${proc_swap}"
        sleep 1
    done
) &
MONITOR_PID=$!

set +e
wait "${RUN_PID}"
RUN_STATUS=$?
wait "${MONITOR_PID}" 2>/dev/null
set -e

if [[ -s "${CSV_PATH}" ]] && grep -q '^# summary ' "${CSV_PATH}"; then
    python3 "${ROOT_DIR}/scripts/metrics_to_json.py" "${CSV_PATH}" "${JSON_PATH}" "${MONITOR_PATH}"
    echo "Summary telemetry: ${JSON_PATH}"
fi

trap - INT TERM
exit "${RUN_STATUS}"
