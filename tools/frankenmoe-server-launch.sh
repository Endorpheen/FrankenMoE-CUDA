#!/usr/bin/env bash
set -euo pipefail

dir=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
server="$dir/frankenmoe-server.bin"

export FRANKENMOE_WEB_ROOT="${FRANKENMOE_WEB_ROOT-$dir/web}"
if [[ ! -t 1 || ${TERM:-dumb} == dumb || ${FRANKENMOE_CONSOLE:-1} == 0 ]] ||
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
if [[ -n ${FRANKENMOE_LOG:-} ]]; then
    console+=(--save "$FRANKENMOE_LOG")
fi
exec python3 "$dir/frankenmoe-console.py" "${console[@]}" -- "$server" "$@"
