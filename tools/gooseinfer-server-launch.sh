#!/usr/bin/env bash
set -euo pipefail

dir=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
server="$dir/gooseinfer-server.bin"
if [[ ! -x "$server" ]]; then server="$dir/frankenmoe-server.bin"; fi

export GOOSEINFER_WEB_ROOT="${GOOSEINFER_WEB_ROOT-${FRANKENMOE_WEB_ROOT-$dir/web}}"
export FRANKENMOE_WEB_ROOT=$GOOSEINFER_WEB_ROOT
if [[ ! -t 1 || ${TERM:-dumb} == dumb || ${GOOSEINFER_CONSOLE:-${FRANKENMOE_CONSOLE:-1}} == 0 ]] ||
    ! command -v python3 >/dev/null; then
    exec "$server" "$@"
fi

args=("$@")
while (( ${#args[@]} >= 2 )); do
    case "${args[0]}" in
        --ctx-size|--batch-size|--ubatch-size|--cache-type-k|--cache-type-v|--expert-cache-mib|\
        --threads|--threads-batch|--cpu-mask|--cpu-mask-batch|--cpu-strict|--cpu-strict-batch)
            args=("${args[@]:2}")
            ;;
        *) break ;;
    esac
done

if (( ${#args[@]} < 3 || ${#args[@]} > 4 )) || [[ ${args[1]} != --serve ]]; then
    exec "$server" "$@"
fi

console=(--model "${args[0]}")
log=${GOOSEINFER_LOG:-${FRANKENMOE_LOG:-}}
if [[ -n "$log" ]]; then
    console+=(--save "$log")
fi
exec python3 "$dir/gooseinfer-console.py" "${console[@]}" -- "$server" "$@"
