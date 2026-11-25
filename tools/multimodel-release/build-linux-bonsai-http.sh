#!/usr/bin/env bash
set -euo pipefail

src=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$src/../.." && pwd)
out=${1:?Specify a fresh Bonsai HTTP build directory}
export FRANKENMOE_CHECKPOINTS=${FRANKENMOE_CHECKPOINTS:-$root/build}
export FRANKEN_MAIN=${FRANKEN_MAIN:-$root}
experiment="$root/experiments/EXP-2026-11-25-2019-bonsai-mmq-pipeline"

mkdir "$out"
bash "$experiment/source/build.sh" "$out/device" \
    "$FRANKENMOE_CHECKPOINTS/bonsai-flash-kv-panels-2018-lean-rebuild"
bash "$experiment/source/build-runtime.sh" "$out/http" "$out/device"

readelf -d "$out/http/frankenmoe-infer" > "$out/http/dependencies.txt"
if grep -Eq 'NEEDED.*(libstdc\+\+|libllama|libggml)' "$out/http/dependencies.txt"; then
    printf 'Unexpected dependency in the Bonsai HTTP runtime\n' >&2
    exit 1
fi

receipt="$experiment/data/SHA256SUMS"
(
    cd "$out/http"
    awk '/\/frankenmoe-infer$/ {print $1 "  frankenmoe-infer"} /\/pipeline.cubin$/ {print $1 "  pipeline.cubin"}' \
        "$receipt" | sha256sum --strict -c -
)
