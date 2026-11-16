#!/usr/bin/env bash
# The only way a benchmark server is allowed to start.
#
# Why this exists: on 2026-11-16 overlapping servers produced hours of wrong conclusions and three wrong explanations
# (the KV cache, the GPU, the build). Every failed start that did not exit left a server behind; the next start raced it
# for the port and for device memory; `cudaMalloc` failed; the number was read as a property of the code.
#
# A rule in a document did not stop it. This script does: it refuses to start when the port already has an owner, and
# it refuses to guess when it cannot prove the port is free.
#
# Usage:
#   bash tools/run-once.sh <port> <tag> [len] [b] [ub] [cache]
set -euo pipefail

PORT=${1:?usage: run-once.sh <port> <tag> [len] [b] [ub] [cache]}
TAG=${2:?tag required}

# 1. Who owns the port? By pid, not by process name - `pgrep llama-server` misses a leftover running under another name
#    and this is exactly how a real leftover was missed for three hours.
if ss -ltn 2>/dev/null | grep -qE ":${PORT}\$"; then
    OWNER=$(ss -ltnp 2>/dev/null | awk -v p=":${PORT}" '$4 ~ p {print $0}' | grep -oE 'pid=[0-9]+' | cut -d= -f2 | head -1)
    echo "REFUSING TO START: port ${PORT} already has an owner (pid ${OWNER:-unknown})."
    echo "Stop that process and verify the port is released before running again."
    exit 2
fi

# 2. Any server of ours still alive, whatever its name.
# Only OUR servers: stage-run/llama-server from the reconstructed build. Igor's embeddings server
# (igorek-llama-server) and anything else under another path are never counted and never touched.
OURS='stage-run/llama-server'
LEFT=$(pgrep -af "${OURS}" 2>/dev/null | grep -v 'run-once' | wc -l || true)
if [ "${LEFT}" -gt 0 ]; then
    echo "REFUSING TO START: ${LEFT} of our server process(es) still alive:"
    pgrep -af "${OURS}" 2>/dev/null | grep -v 'run-once' | head -5
    echo "Stop them and verify they exited before running again."
    exit 2
fi

# 3. The model must be resident, or the number describes the disk.
MODEL=${MODEL:-/home/end0/AI/LocalAI/Models/Apex-35B-V1.0-Q4_K_M.gguf}
if [ -x /tmp/res ] && [ -f "${MODEL}" ]; then
    RES=$(/tmp/res "${MODEL}" 2>/dev/null | head -1)
    echo "model residency: ${RES}"
    case "${RES}" in
        *"100.0%"*|*" 9"*"%") ;;
        *) echo "WARNING: residency below ~99% - the measurement will describe the disk, not the runtime (rule 23)" ;;
    esac
fi

echo "port ${PORT} free, no leftover server, starting once"
exec /tmp/len5.sh "$@"
