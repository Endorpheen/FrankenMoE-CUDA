#!/usr/bin/env bash
set -euo pipefail

src=$(cd "$(dirname "$0")" && pwd)
root=$(git rev-parse --show-toplevel)
out=${1:?Specify a fresh Windows build directory}
obj=${2:?Specify the Windows Inkling COFF checkpoint directory}
base=${3:?Specify the Windows common COFF checkpoint directory}
chat=${4:?Specify the Windows v0.12.1 COFF checkpoint directory}
platform=${FRANKEN_MAIN:?Set FRANKEN_MAIN to the checkout with Win32 host headers}
cc=${WINDOWS_CC:-x86_64-w64-mingw32-gcc-posix}
objcopy=${WINDOWS_OBJCOPY:-x86_64-w64-mingw32-objcopy}
cuda=${CUDA_HOME:-/usr/local/cuda}
bonsai="$root/build/bonsai-mmq-k64-2000-default"
http="$root/build/bonsai-http-2003-link"
session="$root/experiments/EXP-2026-11-25-2002-bonsai-session/source"
apex="$root/build/checkpoint-apex-exp1880-before-inkling"
inkling="$root/build/checkpoint-inkling-exp1969-before-bonsai"
previous="$root/build/inkling-apex-gate/single-link-1953"
device="$root/build/inkling-exp-baseline/device-attention"
mkdir "$out"

flags=(-O2 -std=gnu11 -D_GNU_SOURCE -D_WIN32_WINNT=0x0a00 -DI2_NUMERIC_MODULE
       -ffunction-sections -fdata-sections -Werror=implicit-function-declaration)
inc=(-I "$inkling/source" -I "$apex/checkpoint" -I "$apex/build-inputs" -I "$previous"
     -I "$device/inkling-attention-device-input" -I "$device/inkling-completion" -I "$device/inkling-moe-tile32"
     -I "$session" -I "$platform/vendor/flint/ggml/include" -I "$platform/vendor/flint/ggml/src"
     -I "$platform/integration/goose-standalone" -I "$platform/integration/m1/host")

python3 "$src/prepare-windows-server.py" "$http/model-host.c" "$obj/infer-main.c" "$out"
python3 "$src/port-bonsai-windows.py" "$root/build/bonsai-mmq-pipeline-2019-http/backend.c" "$out/bonsai-http.c"
"$cc" "${flags[@]}" -I "$bonsai" -I "$session" -I "$cuda/include" -Dmain=bonsai_diagnostic_main \
    -c "$out/bonsai-http.c" -o "$out/bonsai-session-raw.o"
"$objcopy" --redefine-syms="$http/rename.txt" "$out/bonsai-session-raw.o" "$out/bonsai-session.o"
"$cc" "${flags[@]}" -w -c "$bonsai/ffn-tail.c" -o "$out/bonsai-goose-raw.o"
"$objcopy" --redefine-syms="$http/rename.txt" "$out/bonsai-goose-raw.o" "$out/bonsai-goose-renamed.o"
cut -d' ' -f2 "$http/rename.txt" > "$out/bonsai-exports.txt"
"$objcopy" --keep-global-symbols="$out/bonsai-exports.txt" "$out/bonsai-goose-renamed.o" "$out/bonsai-goose.o"
"$cc" "${flags[@]}" "${inc[@]}" -c "$out/model-host-multimodel.c" -o "$out/model-host.o"
"$cc" "${flags[@]}" "${inc[@]}" -c "$out/infer-main-multimodel.c" -o "$out/infer-main.o"

"$objcopy" --keep-global-symbol=inkling_tokenizer_init --keep-global-symbol=bt_tokenize_inkling2 \
    "$obj/tokenizer.o" "$out/tokenizer.o"

"$cc" -static -municode -Wl,--gc-sections,--no-insert-timestamp "$out/"{infer-main,model-host,bonsai-session,bonsai-goose,tokenizer}.o \
    "$obj/"{cuda-driver,graph-execute,inkling-backend,inkling-model-visible,numeric,lfu,prior,bucket,sampler-scan}.o "$chat/main-windows.o" \
    "$base/"{cpu,graph-bindings,libc-forwards,slru-shim,goose-host,ggml,ggml-quants,ggml-system,probability,probability-pipeline,platform,planner-link,sampler-scan}.o \
    "$chat/web.o" -lws2_32 -lpsapi -lwinpthread -lm -o "$out/gooseinfer-multimodel.exe"
cp "$out/gooseinfer-multimodel.exe" "$out/frankenmoe-multimodel.exe"
