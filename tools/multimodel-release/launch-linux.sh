#!/usr/bin/env bash
set -euo pipefail

root=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
name=${1:-}
if [[ "$name" != apex && "$name" != inkling && "$name" != bonsai ]]; then
    printf 'Usage: %s apex|inkling|bonsai [model.gguf] [port]\n' "$0" >&2
    exit 2
fi
model_dir=${FRANKENMOE_MODELS_DIR:-$HOME/AI/LocalAI/Models}

case "$name" in
    apex) default_model=Apex-35B-V1.0-Q4_K_M.gguf; default_port=9101 ;;
    inkling) default_model=Inkling-2-Mini-AD-IQ4_XS.gguf; default_port=9102 ;;
    bonsai) default_model=Ternary-Bonsai-2.5-27B-PQ2_0-mtp.gguf; default_port=9103 ;;
esac
model=${2:-$model_dir/$default_model}
port=${3:-$default_port}
if [[ ! -r "$model" || ! "$port" =~ ^[0-9]+$ || ${#port} -gt 5 ]] ||
   (( 10#$port < 1 || 10#$port > 65535 )); then
    printf 'Invalid model path or TCP port: %s %s\n' "$model" "$port" >&2
    exit 2
fi

server="$root/bin/$name-server"
mapfile -t argv < "$root/profiles/$name.argv"
while IFS='=' read -r key value; do
    if [[ -n "$key" && ! -v "$key" ]]; then export "$key=$value"; fi
done < "$root/profiles/$name.env"

export FRANKENMOE_WEB_ROOT=${FRANKENMOE_WEB_ROOT:-$root/web}
if [[ "$name" == bonsai ]]; then
    export BONSAI_CUDA_IMAGE="${BONSAI_CUDA_IMAGE:-$root/cuda/bonsai-http.cubin}"
    export BONSAI_FROZEN_PTX="${BONSAI_FROZEN_PTX:-$root/cuda/frozen-goose.ptx}"
fi

if [[ -t 1 && ${TERM:-dumb} != dumb && ${FRANKENMOE_CONSOLE:-1} != 0 ]] && command -v python3 >/dev/null; then
    options=(--model "$model")
    if [[ -n ${FRANKENMOE_LOG:-} ]]; then options+=(--save "$FRANKENMOE_LOG"); fi
    exec python3 "$root/frankenmoe-console.py" "${options[@]}" -- "$server" "${argv[@]}" "$model" --serve "$port"
fi
exec "$server" "${argv[@]}" "$model" --serve "$port"
