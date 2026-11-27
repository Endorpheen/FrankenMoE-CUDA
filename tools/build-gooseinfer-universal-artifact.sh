#!/usr/bin/env bash
set -euo pipefail

target=${1:?Usage: build-gooseinfer-universal-artifact.sh linux|windows OUTPUT_DIR}
out=$(realpath -m -- "${2:?Missing output directory}")
tools=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
case "$target" in
    linux) exe=gooseinfer; giga=libgiga-goose.a ;;
    windows) exe=gooseinfer.exe; giga=giga-unit.o ;;
    *) printf 'Unknown GooseInfer target: %s\n' "$target" >&2; exit 2 ;;
esac
bash "$tools/build-gooseinfer-universal-cuda-host.sh" \
    "$target" "$out/cuda" "$out/cuda-images" "$out/cpu" "$out/bonsai-model"

inputs=(
    "$out/entry.o" "$out/cuda/cuda-entry.o" "$out/vulkan/vulkan-backend.o"
    "$out/inkling/numeric.o" "$out/bonsai/model-host.o" "$out/gigachat4/$giga"
    "$out/cpu/cpu.o" "$out/cuda/cuda-driver.o" "$out/inkling/graph-execute.o"
    "$out/inkling/inkling-backend.o" "$out/inkling/lfu.o" "$out/bonsai-model/main.o"
    "$out/bonsai/bonsai-session.o" "$out/bonsai/mtp-goose.o" "$out/bonsai/bonsai-goose.o"
)
if [[ $target == linux ]]; then
    archive="$out/common/libgooseinfer-common.a"
    archive_tmp=$(mktemp -d "$out/common/.gooseinfer-common.XXXXXX")
    trap 'rm -rf -- "$archive_tmp"' EXIT
    "${AR:-ar}" rcs "$archive_tmp/libgooseinfer-common.a" \
        "$out/common/"{graph-bindings,libc-forwards,slru-shim,goose-host,ggml,ggml-system,probability,probability-pipeline,sampler-scan,planner-link}.o \
        "$out/inkling-model/inkling-model.o" "$out/inkling/"{prior,bucket,tokenizer}.o
    mv -- "$archive_tmp/libgooseinfer-common.a" "$archive"
    inputs+=("$archive" "$out/bonsai-images.o")
else
    inputs+=("$out/bonsai-images.o"
             "$out/common/"{platform,socket-forwards,web,graph-bindings,goose-host,slru-shim,ggml-system,ggml,probability,probability-pipeline,sampler-scan,planner-link}.o
             "$out/inkling-model/inkling-model.o" "$out/inkling/"{inkling-sampler-scan,prior,bucket,tokenizer}.o)
fi
printf '%s\n' "${inputs[@]}" > "$out/link-inputs.argv"
bash "$tools/link-gooseinfer-universal.sh" "$target" "$out/link-inputs.argv" "$out/$exe"
(cd -- "$out" && sha256sum "$exe" > SHA256SUMS)
