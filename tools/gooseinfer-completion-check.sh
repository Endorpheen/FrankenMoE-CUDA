#!/usr/bin/env bash
set -euo pipefail

base=${1:?Usage: gooseinfer-completion-check.sh BASE_URL REQUEST_JSON}
fixture=${2:?Missing request JSON path}
ready_timeout=${GOOSEINFER_READY_TIMEOUT:-600}
request_timeout=${GOOSEINFER_REQUEST_TIMEOUT:-220}
if [[ ! $ready_timeout =~ ^[1-9][0-9]*$ || ! $request_timeout =~ ^[1-9][0-9]*$ ]]; then
    printf 'Timeouts must be positive whole seconds.\n' >&2
    exit 2
fi
base=${base%/}
payload=$(mktemp)
trap 'rm -f -- "$payload"' EXIT
jq -ce 'select(type == "object")' "$fixture" >"$payload"

deadline=$((SECONDS + ready_timeout))
until curl -fsS --noproxy '*' --max-time 2 "$base/health" >/dev/null 2>&1; do
    if (( SECONDS >= deadline )); then
        printf 'GooseInfer did not become ready at %s/health in %s seconds. Check its startup log.\n' \
            "$base" "$ready_timeout" >&2
        exit 3
    fi
    sleep 1
done

curl -fsS --noproxy '*' --max-time "$request_timeout" \
    -H 'Content-Type: application/json' --data-binary "@$payload" "$base/completion"
