#!/usr/bin/env bash
# Launch the expert-tier llama-server for Qwen3.8-Flash-Next UD-IQ3_XXS.
# The standard llama.cpp web UI and OpenAI-compatible API are exposed on the same port.
# Usage: scripts/run_qwen38_server.sh /path/to/Qwen...-00001-of-00003.gguf
# Environment: PORT, HOST, CTX_SIZE, EHS (0 = off, -1 = experimental autofit), SWAPS_PER_TOK,
# VRAM_RESERVE_MB, THREADS, EXTRA_ARGS.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
# Default to the patched expert-tier build (state B in docs/RUN_QWEN38.md): identical outputs and
# speed to the clean fork, minus 12.9 GiB peak RSS (EXP-2026-09-01-006). Build it with
# scripts/prepare_upstreams.sh, then cmake -S work/llama.cpp-integration -B build/expert-tier-franken-cuda.
# Set BUILD_DIR=build/expert-tier-cuda to launch the clean public fork instead.
BUILD_DIR="${BUILD_DIR:-${ROOT_DIR}/build/expert-tier-franken-cuda}"
SERVER_BIN="${BUILD_DIR}/bin/llama-server"
[[ -x "${SERVER_BIN}" ]] || { echo "Missing llama-server: ${SERVER_BIN} (see scripts/build.sh)" >&2; exit 2; }

MODEL_PATH="${1:-${MODEL_PATH:-}}"
[[ -n "${MODEL_PATH}" ]] || { echo "Usage: $0 /path/to/Qwen...-00001-of-00003.gguf" >&2; exit 2; }
for part in 00001 00002 00003; do
    shard="${MODEL_PATH/00001-of-00003/${part}-of-00003}"
    [[ -s "${shard}" ]] || { echo "Missing shard: ${shard}" >&2; exit 2; }
done

MEM_AVAILABLE_KIB="$(awk '/MemAvailable:/ {print $2}' /proc/meminfo)"
if (( MEM_AVAILABLE_KIB < 8 * 1024 * 1024 )); then
    echo "Insufficient MemAvailable: at least 8 GiB of headroom is required" >&2
    exit 3
fi

# Autofit remains opt-in because EXP-010 found a decode regression and unstable greedy output.
# The reserve leaves room for GPU processes that start after the server has initialized.
VRAM_RESERVE_MB="${VRAM_RESERVE_MB:-2048}"
EHS="${EHS:-0}"

PORT="${PORT:-8080}"
HOST="${HOST:-127.0.0.1}"
CTX_SIZE="${CTX_SIZE:-8192}"
THREADS="${THREADS:-16}"

mkdir -p "${ROOT_DIR}/results"
LOG_PATH="${LOG_PATH:-${ROOT_DIR}/results/qwen38-server.log}"
MONITOR_PATH="${MONITOR_PATH:-${ROOT_DIR}/results/qwen38-server-monitor.csv}"

echo "timestamp,rss_kib,process_swap_kib,gpu_used_mib,gpu_temp_c,read_bytes,system_pswpin,system_pswpout" >"${MONITOR_PATH}"

# Cold startup legitimately maps and reads the 82 GB model, so the server's swap watchdog uses
# a wider transient threshold than the single-run launcher.
"${SERVER_BIN}" \
    -m "${MODEL_PATH}" \
    -ngl 99 \
    --cpu-moe \
    -ehs "${EHS}" \
    --ehs-reserve-mb "${VRAM_RESERVE_MB}" \
    -ot "per_layer_token_embd.weight=CPU" \
    -c "${CTX_SIZE}" -fa on --jinja \
    -t "${THREADS}" \
    --host "${HOST}" --port "${PORT}" \
    ${EXTRA_ARGS:-} \
    >"${LOG_PATH}" 2>&1 &
RUN_PID=$!

stop_children() {
    if [[ -n "${RUN_PID:-}" ]] && kill -0 "${RUN_PID}" 2>/dev/null; then
        kill -INT "${RUN_PID}" 2>/dev/null || true
        sleep 5
        kill -TERM "${RUN_PID}" 2>/dev/null || true
        sleep 3
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
        gpu="$(nvidia-smi --query-gpu=memory.used,temperature.gpu --format=csv,noheader,nounits | head -1 | tr -d ' ')"
        cur_in="$(awk '$1 == "pswpin" {print $2}' /proc/vmstat)"
        cur_out="$(awk '$1 == "pswpout" {print $2}' /proc/vmstat)"
        printf '%(%s)T,%s,%s,%s,%s,%s,%s\n' -1 "${rss:-0}" "${proc_swap:-0}" "${gpu:-0,0}" \
            "${read_bytes:-0}" "${cur_in:-0}" "${cur_out:-0}" >>"${MONITOR_PATH}"
        proc_burst=$(( proc_swap - last_proc_swap ))
        if (( proc_burst > 262144 )); then
            echo "Process swap grew by $(( proc_burst * 4 / 1024 )) MiB in one second; stopping server" >&2
            stop_children
            break
        fi
        sys_rate=$(( (cur_in - last_in + cur_out - last_out) * 4 / 1024 ))
        if (( sys_rate > 2097152 )) || { (( sys_rate > 512 )) && (( ++burst_secs >= 5 )); }; then
            echo "Sustained system swap I/O at ${sys_rate} MiB/s; stopping server" >&2
            stop_children
            break
        fi
        (( sys_rate > 512 )) || burst_secs=0
        last_in="${cur_in}"
        last_out="${cur_out}"
        last_proc_swap="${proc_swap}"
        sleep 1
    done
) &
MONITOR_PID=$!

echo "llama-server pid ${RUN_PID}, log: ${LOG_PATH}, telemetry: ${MONITOR_PATH}"
echo "Web UI: http://${HOST}:${PORT} (wait for 'server is listening' in the log)"

wait "${RUN_PID}"
RUN_STATUS=$?
set +e
wait "${MONITOR_PID}" 2>/dev/null
set -e
trap - INT TERM
exit "${RUN_STATUS}"
