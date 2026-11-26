#!/usr/bin/env bash
set -euo pipefail

root=$(cd "$(dirname "$0")/.." && pwd)
source_dir="$root/integration/goose-standalone/vulkan"
out=${1:?Specify the output directory}
target=${2:-linux}
mode=${3:-probe}
[[ "$mode" == probe || "$mode" == compute || "$mode" == qwen2 ]]
mkdir -p "$out"
flags=(-std=c11 -O2 -Wall -Wextra -Werror)
headers=${VULKAN_HEADERS:-/usr/include}
case "$target" in
    linux)
        cc=${CC:-cc}
        executable=frankenmoe-vulkan-probe
        libs=(-ldl)
        ;;
    windows)
        cc=${WINDOWS_CC:-x86_64-w64-mingw32-gcc}
        executable=frankenmoe-vulkan-probe.exe
        libs=(-static)
        ;;
    *)
        printf '%s\n' 'Expected linux or windows' >&2
        exit 2
        ;;
esac
test -r "$headers/vulkan/vulkan.h"
# Keep the host libc headers out of the MinGW include path.
mkdir -p "$out/vulkan-headers"
cp -R "$headers/vulkan" "$out/vulkan-headers/"
if [[ -d "$headers/vk_video" ]]; then cp -R "$headers/vk_video" "$out/vulkan-headers/"; fi
"$cc" "${flags[@]}" -isystem "$out/vulkan-headers" \
    "$source_dir/"{loader,device,memory,queue,probe}.c \
    "${libs[@]}" -o "$out/$executable"

if [[ "$mode" != probe ]]; then
    glslang=${GLSLANG_VALIDATOR:-glslangValidator}
    spirv_val=${SPIRV_VAL:-spirv-val}
    suffix=${executable#frankenmoe-vulkan-probe}
    for shader in rmsnorm q4_0 swiglu rope kv_store attention q8_0 gather_q4_0 bias add; do
        "$glslang" -V --target-env vulkan1.2 -S comp "-DOP_${shader^^}" \
            "$source_dir/shaders/reference.comp" -o "$out/$shader.spv"
        "$spirv_val" --target-env vulkan1.2 "$out/$shader.spv"
    done
    python3 "$root/tools/embed-vulkan-shaders.py" "$out"
    "$cc" "${flags[@]}" -isystem "$out/vulkan-headers" -I"$out" -I"$source_dir" \
        "$source_dir/"{loader,device,memory,queue,graph}.c \
        "$root/integration/goose-standalone/tests/vulkan/"{compute,attention,projection}.c \
        "${libs[@]}" -lm -o "$out/frankenmoe-vulkan-compute-test$suffix"
fi

if [[ "$mode" == qwen2 ]]; then
    goose=${GOOSE_BIN:-goose}
    cat "$root/goose/misc/gguf-headers.goose" "$root/goose/model/"{gguf-tensors,gguf-reader}.goose \
        "$source_dir/qwen2-step.goose" > "$out/qwen2_unit.goose"
    "$goose" --standalone -o "$out/qwen2_unit.c" --header "$out/qwen2_unit.h" "$out/qwen2_unit.goose"
    "$cc" -std=c11 -O2 -w -ffunction-sections -fdata-sections -c "$out/qwen2_unit.c" -o "$out/qwen2_unit.o"
    "$cc" "${flags[@]}" -isystem "$out/vulkan-headers" -I"$out" -Wl,--gc-sections \
        "$source_dir/"{loader,device,memory,queue,graph,upload,qwen2,infer-main}.c "$out/qwen2_unit.o" \
        "${libs[@]}" -lm -pthread -o "$out/frankenmoe-vulkan$suffix"
fi
