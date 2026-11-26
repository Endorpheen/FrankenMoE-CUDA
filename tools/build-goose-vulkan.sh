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
        libs=(-static -Wl,--no-insert-timestamp)
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
    for shader in rmsnorm q4_0 swiglu rope kv_store attention q8_0 gather_q4_0 bias add pq2_0 gather_pq2_0 fwht causal_conv qk_norm gdn gdn_gates gated_rms head_slice sigmoid_gate kv_store_q4 attention_q4 q4_k q6_k gather_q4_k iq4_nl iq4_xs gather_q8_0 f32_project moe_route; do
        "$glslang" -V --target-env vulkan1.2 -S comp "-DOP_${shader^^}" \
            "$source_dir/shaders/reference.comp" -o "$out/$shader.spv"
        "$spirv_val" --target-env vulkan1.2 "$out/$shader.spv"
    done
    python3 "$root/tools/embed-vulkan-shaders.py" "$out"
    "$cc" "${flags[@]}" -isystem "$out/vulkan-headers" -I"$out" -I"$source_dir" \
        "$source_dir/"{loader,device,memory,queue,graph,upload,bonsai-layer}.c \
        "$root/integration/goose-standalone/tests/vulkan/"{compute,attention,projection,moe-quant,moe-router,bonsai,recurrent,bonsai-layer}.c \
        "${libs[@]}" -lm -o "$out/frankenmoe-vulkan-compute-test$suffix"
fi

if [[ "$mode" == qwen2 ]]; then
    goose=${GOOSE_BIN:-goose}
    python3 - "$root/goose/misc/pretokenizer-qwen35.goose" "$out/pretokenizer-vulkan.goose" <<'PY'
from pathlib import Path
import sys

source, output = map(Path, sys.argv[1:])
text = source.read_text()
anchor = "fn pt_scan(cpts: i64[:], n: i64, seg: i64[:], cap: i64) -> i64 {"
if text.count(anchor) != 1 or text.count("(PT_LETTER | PT_MARK)") != 3 or text.count("& PT_PUNCT") != 2:
    raise ValueError("Qwen pre-tokenizer anchor changed")
text = text.replace("(PT_LETTER | PT_MARK)", "letters").replace("& PT_PUNCT", "& punct")
setup = ('extern "fg_vk_tokenizer_marks" fn pt_model_marks() -> i64;\n' + anchor +
         '\n    let letters = PT_LETTER | (if pt_model_marks() != 0 { PT_MARK } else { 0 });' +
         '\n    let punct = PT_SPACE | letters | PT_NUM;')
output.write_text(text.replace(anchor, setup))
PY
    python3 "$root/tools/compose-vulkan-http.py" "$root" "$out/http_helpers.goose"
    bootstrap="$root/experiments/EXP-2026-11-25-1973-bonsai-pq2-bootstrap/source"
    patch -s "$root/goose/model/gguf-tensors.goose" -o "$out/gguf-tensors-vulkan.goose" < "$bootstrap/gguf-tensors.goose.patch"
    sed -n '/^fn mr_name(/,/^fn mr_moe_name(/{ /^fn mr_moe_name(/d;p; }' \
        "$root/goose/model/qwen35-model-loader.goose" > "$out/bonsai-model-names.goose"
    cat "$root/goose/misc/gguf-headers.goose" "$out/gguf-tensors-vulkan.goose" "$root/goose/model/gguf-reader.goose" \
        "$out/bonsai-model-names.goose" "$bootstrap/bonsai-layout.goose" \
        "$root/goose/misc/"{unicode-tables,unicode-text}.goose "$out/pretokenizer-vulkan.goose" \
        "$root/goose/model/"{bpe-tokenize,bpe-detokenize,tokenizer-engine}.goose \
        "$root/goose/server/"{http-request,http-listener,route-table,jsonparse}.goose "$out/http_helpers.goose" \
        "$source_dir/"{qwen2-step,bonsai-step,http}.goose > "$out/qwen2_unit.goose"
    "$goose" --standalone -o "$out/qwen2_unit.c" --header "$out/qwen2_unit.h" "$out/qwen2_unit.goose"
    "$cc" -std=c11 -O2 -w -ffunction-sections -fdata-sections -c "$out/qwen2_unit.c" -o "$out/qwen2_unit.o"
    standalone="$root/integration/goose-standalone"
    if [[ "$target" == windows ]]; then
        socket=("$standalone/windows/"{socket-forwards,platform}.c)
        libs+=(-lws2_32 -lpsapi)
    else
        socket=("$standalone/libc-forwards.c")
    fi
    "$cc" "${flags[@]}" -isystem "$out/vulkan-headers" -I"$out" -Wl,--gc-sections \
        "$source_dir/"{loader,device,memory,queue,graph,upload,gguf,qwen2,bonsai-layer,bonsai-model,tokenizer,http,infer-main}.c "$out/qwen2_unit.o" \
        "${socket[@]}" "$standalone/json-strtod-provider.c" \
        "${libs[@]}" -lm -pthread -o "$out/frankenmoe-vulkan$suffix"
fi
