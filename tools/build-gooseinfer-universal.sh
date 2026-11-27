#!/usr/bin/env bash
set -euo pipefail

target=${1:?Usage: build-gooseinfer-universal.sh linux|windows OUTPUT_DIR}
out=$(realpath -m -- "${2:?Missing output directory}")
tools=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
case "$target" in
    linux) exe=gooseinfer; giga=libgiga-goose.a ;;
    windows) exe=gooseinfer.exe; giga=giga-unit.o ;;
    *) printf 'Unknown GooseInfer target: %s\n' "$target" >&2; exit 2 ;;
esac
: "${GOOSE_HOST_BIN:?Set GOOSE_HOST_BIN to Fried Goose KeelWing ownership v2}"
: "${GOOSE_HOST_STDLIB:?Set GOOSE_HOST_STDLIB to the matching KeelWing stdlib directory}"
: "${APEX_ROUTING_GOOSE:?Set APEX_ROUTING_GOOSE to the Goose 2026.3 compiler}"
: "${GOOSE_BIN:?Set GOOSE_BIN to Fried Goose KeelWing v0.3.0 preview}"
: "${APEX_PTXAS:?Set APEX_PTXAS to CUDA 12.4 ptxas}"
required=(python3 patch "$GOOSE_BIN" "$APEX_PTXAS" "$GOOSE_HOST_BIN" "$APEX_ROUTING_GOOSE" "${CUDA_HOME:-/usr/local/cuda}/bin/ptxas"
          "${CLANG:-clang-18}" "${GLSLANG_VALIDATOR:-glslangValidator}" "${SPIRV_VAL:-spirv-val}")
: "${GOOSE_CUDA_DRIVER_BIN:?Set GOOSE_CUDA_DRIVER_BIN to Fried Goose KeelWing v0.3.1-dev.1}"
required+=("$GOOSE_CUDA_DRIVER_BIN")
if [[ $target == linux ]]; then
    required+=("${CC:-gcc-13}" "${GIGACHAT4_CC:-gcc-15}" "${VULKAN_GOOSE_CC:-gcc-15}" "${AR:-ar}" nm objcopy)
else
    prefix=${WINDOWS_TOOL_PREFIX:-x86_64-w64-mingw32-}
    required+=("${WINDOWS_CC:-x86_64-w64-mingw32-gcc}" "${prefix}ar" "${prefix}nm"
               "${prefix}objcopy" "${WINDOWS_OBJCOPY:-x86_64-w64-mingw32-objcopy}")
fi
for tool in "${required[@]}"; do
    if ! command -v "$tool" >/dev/null; then
        printf 'Missing build tool: %s. See docs/BUILD.md for the required version.\n' "$tool" >&2
        exit 2
    fi
done
mkdir -p -- "$out"
if [[ -n $(find "$out" -mindepth 1 -maxdepth 1 -print -quit) ]]; then
    printf 'GooseInfer requires an empty output directory: %s\n' "$out" >&2
    exit 2
fi
export GOOSE_HOME=${GOOSE_HOME:-"$out/tool-cache"}
compiler=$(realpath -- "$(command -v "$GOOSE_HOST_BIN")")
stdlib=$(realpath -- "$GOOSE_HOST_STDLIB")
if [[ ! -r $stdlib/std.goose || ! -r $stdlib/math.goose ]]; then
    printf 'Missing KeelWing standard library in %s. See docs/BUILD.md.\n' "$stdlib" >&2
    exit 2
fi
mkdir -p -- "$out/toolchain/bin"
ln -s -- "$compiler" "$out/toolchain/bin/goose"
ln -s -- "$stdlib" "$out/toolchain/stdlib"
export GOOSE_HOST_BIN="$out/toolchain/bin/goose"
export GOOSE_BIN

bash "$tools/build-gooseinfer-universal-components.sh" "$target" "$out"
bash "$tools/build-gooseinfer-universal-artifact.sh" "$target" "$out"
