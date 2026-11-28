#!/usr/bin/env bash
set -euo pipefail

root=$(cd "$(dirname "$0")/.." && pwd)
source_dir="$root/integration/goose-standalone/vulkan"
out=${1:?Specify the output directory}
target=${2:-linux}
mode=${3:-probe}
[[ "$mode" == probe || "$mode" == compute || "$mode" == qwen2 || "$mode" == embedded ]]
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
python3 "$root/tools/compose-vulkan-dispatch.py" "$source_dir/device.h" "$out"
"$cc" "${flags[@]}" -isystem "$out/vulkan-headers" -I"$source_dir" -x c -fsyntax-only "$out/device-layout.h"
cat "$source_dir/"{queue,upload,memory,json-number,loader,device}.goose \
    "$source_dir/loader-$target.goose" "$out/device-dispatch.goose" > "$out/queue.goose"
"${GOOSE_CUDA_DRIVER_BIN:?Set GOOSE_CUDA_DRIVER_BIN to Fried Goose KeelWing v0.3.1-dev.1}" --standalone \
    -o "$out/queue.c" "$out/queue.goose"
"$cc" -O2 -std=c11 -ffunction-sections -fdata-sections -w \
    -Dgoose_init=fg_vk_queue_init -Dgg_init_globals=fg_vk_queue_globals -c "$out/queue.c" -o "$out/queue.o"
queue_objcopy=${OBJCOPY:-objcopy}
if [[ "$target" == windows ]]; then queue_objcopy=${WINDOWS_OBJCOPY:-x86_64-w64-mingw32-objcopy}; fi
sed -nE 's/^extern "(fg_vk_loader_api_([[:alnum:]_]+))".*/\1 \2/p' \
    "$source_dir/loader-$target.goose" > "$out/loader.symbols"
"$queue_objcopy" --redefine-sym=fg_vulkan_crt_strtod=strtod --redefine-syms="$out/loader.symbols" "$out/queue.o"
"$cc" "${flags[@]}" -isystem "$out/vulkan-headers" \
    "$source_dir/"probe.c "$out/queue.o" \
    "${libs[@]}" -o "$out/$executable"

if [[ "$mode" != probe ]]; then
    seek_symbol=fseeko
    if [[ "$target" == windows ]]; then seek_symbol=_fseeki64; fi
    cat "$source_dir/"{moe-bank,bonsai-layer,graph-plan,graph}.goose > "$out/moe-bank.goose"
    "${GOOSE_CUDA_DRIVER_BIN:?}" --standalone -o "$out/moe-bank.c" "$out/moe-bank.goose"
    "$cc" -O2 -std=c11 -ffunction-sections -fdata-sections -Dgoose_init=fg_vk_bank_init \
        -Dgg_init_globals=fg_vk_bank_globals -c "$out/moe-bank.c" -o "$out/moe-bank.o"
    "$queue_objcopy" --redefine-sym="fg_vk_bank_seek=$seek_symbol" --redefine-sym=fg_vk_bank_read=fread "$out/moe-bank.o"
    glslang=${GLSLANG_VALIDATOR:-glslangValidator}
    spirv_val=${SPIRV_VAL:-spirv-val}
    suffix=${executable#frankenmoe-vulkan-probe}
    for shader in rmsnorm q4_0 swiglu rope kv_store attention q8_0 gather_q4_0 bias add pq2_0 gather_pq2_0 fwht causal_conv qk_norm gdn gdn_gates gated_rms head_slice sigmoid_gate kv_store_q4 attention_q4 q4_k q6_k gather_q4_k iq4_nl iq4_xs gather_q8_0 f32_project moe_route moe_weighted apex_qk_norm gdn_apply sigmoid_gate_scalar kv_fwht inkling_route inkling_situ inkling_attention; do
        "$glslang" -V --target-env vulkan1.2 -S comp "-DOP_${shader^^}" \
            "$source_dir/shaders/reference.comp" -o "$out/$shader.spv"
        "$spirv_val" --target-env vulkan1.2 "$out/$shader.spv"
    done
    python3 "$root/tools/embed-vulkan-shaders.py" "$out"
    "$cc" "${flags[@]}" -isystem "$out/vulkan-headers" -x c -fsyntax-only "$source_dir/graph-layout.h"
    "$cc" "${flags[@]}" -isystem "$out/vulkan-headers" -I"$source_dir" \
        -ffunction-sections -fdata-sections -c "$out/shader-words.c" -o "$out/shader-words.o"
    "$cc" "${flags[@]}" -isystem "$out/vulkan-headers" -I"$out" -I"$source_dir" \
        "$out/queue.o" "$out/moe-bank.o" "$out/shader-words.o" \
        "$root/integration/goose-standalone/tests/vulkan/"{compute,attention,projection,moe-quant,moe-router,apex-experts,bonsai,recurrent,bonsai-layer,inkling-attention}.c \
        "${libs[@]}" -lm -o "$out/frankenmoe-vulkan-compute-test$suffix"
fi

if [[ "$mode" == qwen2 || "$mode" == embedded ]]; then
    "$cc" "${flags[@]}" -isystem "$out/vulkan-headers" -x c -fsyntax-only "$source_dir/qwen2-layout.h"
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
text = text.replace("fn pt_qwen35_body(text: const u8[:], cpts: i64[:], seg: i64[:], enc: u8[:], info: i64[:])",
                    "fn pt_qwen35_body(text: const u8[:], cpts: i64[:], seg: i64[:], enc: u8[:], info: i64[:], marks: i64)")
text = text.replace("pt_scan(cpts, n, seg, text.len)", "pt_scan(cpts, n, seg, text.len, marks)")
text = text.replace("return pt_qwen35_body(text, cpts, seg, enc, info);", "return pt_qwen35_body(text, cpts, seg, enc, info, 1);")
setup = (anchor.replace("cap: i64)", "cap: i64, marks: i64)") +
         '\n    let letters = PT_LETTER | (if marks != 0 { PT_MARK } else { 0 });' +
         '\n    let punct = PT_SPACE | letters | PT_NUM;')
output.write_text(text.replace(anchor, setup))
PY
    python3 "$root/tools/compose-vulkan-http.py" "$root" "$out/http_helpers.goose"
    patch -s "$root/goose/model/gguf-tensors.goose" -o "$out/gguf-tensors-vulkan.goose" < "$source_dir/gguf-tensors.goose.patch"
    sed -n '/^fn mr_text(/,/^fn mr_no_clamp(/{ /^fn mr_no_clamp(/d;p; }; /^fn mr_name(/,/^fn mr_moe_name(/{ /^fn mr_moe_name(/d;p; }' \
        "$root/goose/model/qwen35-model-loader.goose" > "$out/bonsai-model-names.goose"
    python3 - "$root/goose/model/bpe-tokenize.goose" "$out/bpe-tokenize-vulkan.goose" <<'PY'
from pathlib import Path
import sys

source, output = map(Path, sys.argv[1:])
text = source.read_text()
scanner = 'let n_word: i64 = pt_qwen35_body(text, cpts, seg, enc, info);'
word = '        let added: i64 = bt_word(enc, off, len, gguf, tok_off, tok_len, tok_slots,'
if text.count(scanner) != 1 or text.count(word) != 1:
    raise ValueError("Vulkan BPE anchors changed")
body = text.index("fn bt_bpe_body(")
text = text[:body] + text[body:].replace("    info: i64[:]\n", "    info: i64[:], mode: i64\n", 1)
text = text.replace("ids, nout_in, cpts, seg, enc, sym, heap, hc, info);",
                    "ids, nout_in, cpts, seg, enc, sym, heap, hc, info, 1);")
text = text.replace(scanner, '''let inkling = mode == 2;
    let n_word: i64 = if inkling { pt_inkling2_body(text, cpts, seg, enc, info) }
                   else { pt_qwen35_body(text, cpts, seg, enc, info, mode) };''')
output.write_text(text.replace(word, '''        if inkling {
            let direct = bt_token_id(gguf, tok_off, tok_len, tok_slots, enc, off, len);
            if direct >= 0 {
                if nout >= ids.len { return -1; }
                ids[nout] = direct;
                nout += 1;
                w += 1;
                continue;
            }
        }
''' + word))
PY
    python3 - "$root/goose/model/tokenizer-engine.goose" "$out/tokenizer-engine-vulkan.goose" <<'PY_TOKENIZER'
from pathlib import Path
import sys

source, output = map(Path, sys.argv[1:])
text = source.read_text()
anchor = text.index("export fn bt_tokenize(")
prefix, tokenize = text[:anchor], text[anchor:]
if tokenize.count("hc: i64[:], info: i64[:])") != 1 or tokenize.count("sym, heap, hc, info);") != 2:
    raise ValueError("Vulkan tokenizer engine anchors changed")
output.write_text(prefix + tokenize.replace("hc: i64[:], info: i64[:])", "hc: i64[:], info: i64[:], mode: i64)")
    .replace("sym, heap, hc, info);", "sym, heap, hc, info, mode);"))
PY_TOKENIZER
    cat "$root/goose/misc/gguf-headers.goose" "$out/gguf-tensors-vulkan.goose" "$root/goose/model/gguf-reader.goose" \
        "$out/bonsai-model-names.goose" "$source_dir/bonsai-layout.goose" \
        "$root/goose/misc/"{unicode-tables,unicode-text}.goose "$out/pretokenizer-vulkan.goose" \
        "$source_dir/inkling/pretokenizer.goose" "$out/bpe-tokenize-vulkan.goose" \
        "$root/goose/model/bpe-detokenize.goose" "$out/tokenizer-engine-vulkan.goose" \
        "$root/goose/server/"{http-request,http-listener,route-table,jsonparse}.goose "$out/http_helpers.goose" \
        "$source_dir/inkling/"{layout,prepare,layer,loop,step}.goose \
        "$source_dir/"{qwen2-step,bonsai-step,apex-step,http}.goose > "$out/qwen2_unit.goose"
    "$goose" --standalone -o "$out/qwen2_unit.c" --header "$out/qwen2_unit.h" "$out/qwen2_unit.goose"
    "${GOOSE_CC:-$cc}" -std=c11 -O2 -w -ffunction-sections -fdata-sections -c "$out/qwen2_unit.c" -o "$out/qwen2_unit.o"
    standalone="$root/integration/goose-standalone"
    if [[ "$target" == windows ]]; then
        libs+=(-lws2_32 -lpsapi)
    fi
    CC="$cc" WINDOWS_CC="$cc" bash "$root/tools/build-gooseinfer-socket.sh" "$target" "$out/socket-forwards.o"
    seek_symbol=fseeko
    tell_symbol=ftello
    if [[ "$target" == windows ]]; then seek_symbol=_fseeki64; tell_symbol=_ftelli64; fi
    cat "$source_dir/"{gguf,tokenizer,http-adapter,infer-entry}.goose \
        "$source_dir/http-$target.goose" "$source_dir/infer-$target.goose" > "$out/gguf.goose"
    "${GOOSE_CUDA_DRIVER_BIN:?}" -o "$out/gguf.c" "$out/gguf.goose"
    entry_flags=()
    if [[ "$mode" == embedded ]]; then entry_flags=(-Dmain=gooseinfer_vulkan_entry); fi
    "$cc" -O2 -std=c11 -ffunction-sections -fdata-sections "${entry_flags[@]}" \
        -Dgg_init_globals=fg_vk_gguf_globals -c "$out/gguf.c" -o "$out/gguf.o"
    time_symbol=time
    if [[ "$target" == windows ]]; then time_symbol=_time64; fi
    "$queue_objcopy" --redefine-sym="fg_vk_http_time=$time_symbol" --redefine-sym="fg_vk_crt_seek=$seek_symbol" --redefine-sym="fg_vk_crt_tell=$tell_symbol" \
        --redefine-sym=fg_vk_crt_read=fread --redefine-sym=fg_vk_crt_strlen=strlen \
        --redefine-sym=fg_vk_crt_memcmp=memcmp --redefine-sym=fg_vk_tok_fopen=fopen \
        --redefine-sym=fg_vk_tok_fclose=fclose --redefine-sym=fg_vk_tok_perror=perror \
        --redefine-sym=fg_vk_cli_crt_write=fwrite --redefine-sym=fg_vk_cli_crt_ferror=ferror "$out/gguf.o"
    if [[ "$mode" == embedded ]]; then
        objcopy=${OBJCOPY:-objcopy}
        if [[ "$target" == windows ]]; then
            objcopy=${WINDOWS_OBJCOPY:-x86_64-w64-mingw32-objcopy}
        fi
        objects=("$out/queue.o" "$out/socket-forwards.o" "$out/gguf.o" "$out/moe-bank.o" "$out/shader-words.o")
        for source in "$source_dir/"{qwen2,bonsai-model,apex-model,inkling-model}.c; do
            name=$(basename -- "$source" .c)
            "$cc" "${flags[@]}" -ffunction-sections -fdata-sections -isystem "$out/vulkan-headers" -I"$out" \
                -c "$source" -o "$out/embedded-$name.o"
            objects+=("$out/embedded-$name.o")
        done
        "$cc" -r -nostdlib "${objects[@]}" "$out/qwen2_unit.o" -o "$out/vulkan-global.o"
        "$objcopy" --keep-global-symbol=gooseinfer_vulkan_entry "$out/vulkan-global.o" "$out/vulkan-backend.o"
        exit 0
    fi
    "$cc" "${flags[@]}" -isystem "$out/vulkan-headers" -I"$out" -Wl,--gc-sections \
        "$source_dir/"{qwen2,bonsai-model,apex-model,inkling-model}.c "$out/qwen2_unit.o" "$out/queue.o" "$out/socket-forwards.o" "$out/gguf.o" "$out/moe-bank.o" "$out/shader-words.o" \
        "${libs[@]}" -lm -pthread -o "$out/frankenmoe-vulkan$suffix"
fi
