#!/usr/bin/env bash
set -euo pipefail

target=${1:?Usage: build-gooseinfer-universal-components.sh linux|windows OUTPUT_DIR}
out=$(realpath -m -- "${2:?Missing output directory}")
tools=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
root=$(dirname -- "$tools")
standalone="$root/integration/goose-standalone"
export GOOSE_BIN=${GOOSE_BIN:-goose}
export GOOSE=$GOOSE_BIN

case "$target" in
    linux)
        entry_cc=${CC:-gcc-13}
        giga_cc=${GIGACHAT4_CC:-gcc-15}
        vulkan_goose_cc=${VULKAN_GOOSE_CC:-gcc-15}
        prefix=
        ;;
    windows)
        entry_cc=${WINDOWS_CC:-x86_64-w64-mingw32-gcc}
        giga_cc=$entry_cc
        vulkan_goose_cc=${VULKAN_GOOSE_CC:-$entry_cc}
        prefix=${WINDOWS_TOOL_PREFIX:-x86_64-w64-mingw32-}
        ;;
    *) printf 'Unknown GooseInfer target: %s\n' "$target" >&2; exit 2 ;;
esac
mkdir -p -- "$out"

"$entry_cc" -O2 -c "$standalone/universal/main.c" -o "$out/entry.o"
CC="$giga_cc" OBJCOPY="${prefix}objcopy" NM="${prefix}nm" AR="${prefix}ar" \
    bash "$standalone/gigachat4/build-graph.sh" "$out/gigachat4"
CC="$entry_cc" WINDOWS_CC="$entry_cc" GOOSE_CC="$vulkan_goose_cc" bash "$tools/build-goose-vulkan.sh" "$out/vulkan" "$target" embedded
CC="$entry_cc" WINDOWS_CC="$entry_cc" bash "$tools/build-gooseinfer-bonsai-cuda.sh" "$target" "$out/bonsai-images.o"

CC="$entry_cc" WINDOWS_CC="$entry_cc" bash "$tools/build-gooseinfer-universal-cpu.sh" "$target" "$out/cpu"

CC="$entry_cc" WINDOWS_CC="$entry_cc" bash "$tools/build-gooseinfer-inkling-model.sh" "$target" "$out/inkling-model"
CC="$entry_cc" WINDOWS_CC="$entry_cc" bash "$tools/build-gooseinfer-inkling-primitives.sh" \
    "$target" "$out/inkling" "$out/inkling-model"
CC="$entry_cc" WINDOWS_CC="$entry_cc" bash "$tools/build-gooseinfer-inkling-host.sh" \
    "$target" "$out/inkling" "$out/inkling-model" "$out/cpu"
CC="$entry_cc" WINDOWS_CC="$entry_cc" bash "$tools/build-gooseinfer-common-sampling.sh" \
    "$target" "$out/common" "$out/inkling-model"
CC="$entry_cc" WINDOWS_CC="$entry_cc" bash "$tools/build-gooseinfer-bonsai-graphs.sh" "$target" "$out/bonsai"
CC="$entry_cc" WINDOWS_CC="$entry_cc" bash "$tools/build-gooseinfer-bonsai-model.sh" "$target" "$out/bonsai-model"
CC="$entry_cc" WINDOWS_CC="$entry_cc" bash "$tools/build-gooseinfer-bonsai-session.sh" \
    "$target" "$out/bonsai" "$out/bonsai-model"
bash "$tools/build-gooseinfer-shared-cuda-image.sh" "$out/cuda-images/goose-cuda-image.h"
bash "$tools/build-gooseinfer-inkling-cuda-images.sh" "$out/cuda-images"
bash "$tools/build-gooseinfer-apex-routing-images.sh" "$out/cuda-images"
bash "$tools/build-gooseinfer-apex-attention-images.sh" "$out/cuda-images"
bash "$tools/build-gooseinfer-apex-extra-images.sh" "$out/cuda-images"
bash "$tools/build-gooseinfer-gigachat4-images.sh" "$out/cuda-images"
CC="$entry_cc" WINDOWS_CC="$entry_cc" bash "$tools/build-gooseinfer-common-core.sh" \
    "$target" "$out/common" "$out/bonsai-model"
bash "$tools/build-gooseinfer-apex-up-gate-images.sh" "$out/cuda-images"
