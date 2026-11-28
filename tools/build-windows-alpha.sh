#!/usr/bin/env bash
# Cross-build the Windows x64 host against a pinned standalone Goose checkpoint.
set -euo pipefail
root=$(cd "$(dirname "$0")/.." && pwd)
out=${1:?usage: build-windows-alpha.sh out-dir Goose-unit CPU-artifacts CUDA-artifacts probability-artifacts}
unit=${2:?directory containing the cumulative main.goose}
cpu=${3:?directory containing cpu.c, cpu.h and ggml-version.h}
cuda=${4:?directory containing the approved embedded goose-cuda-image.h}
prob=${5:?directory containing the Goose probability worker and pipeline ABI}
goose=${GOOSE:?set GOOSE to the pinned Fried Goose compiler}
cc=${WINDOWS_CC:-x86_64-w64-mingw32-gcc-posix}
src="$root/integration/goose-standalone"
ggml="$root/vendor/flint/ggml"
mkdir -p "$out"
out=$(cd "$out" && pwd)
sha256sum "$unit/main.goose" "$cpu/cpu.c" "$cpu/cpu.h" "$cuda/goose-cuda-image.h" \
 "$prob/probability.c" "$prob/probability-pipeline.c" "$goose" > "$out/build-inputs.sha256"
sum_file="$out/build-inputs.sha256"
sha256sum -c "$sum_file"

python3 "$root/tools/compose-alpha-runtime.py" "$unit/main.goose" \
 "$root/goose/server/inference-http.goose" "$out/main.goose"
cp "$cpu"/{cpu.h,ggml-version.h} "$out/"
cp "$cuda/goose-cuda-image.h" "$out/"
printf '#include "main.h"\n' > "$out/goose-unit.h"
ulimit -s 65536
"$goose" --ast-budget-mib 2048 --header "$out/main.h" -o "$out/main-windows.c" "$out/main.goose"
flags=(-O1 -std=c11 -D_GNU_SOURCE -D_WIN32_WINNT=0x0a00 -DWIN32_LEAN_AND_MEAN
       -ffunction-sections -fdata-sections -I "$out" -I "$src" -I "$ggml/include"
       -I "$ggml/src" -I "$ggml/src/ggml-cpu" -I "$root/integration/m1/host")
build() { "$cc" "${flags[@]}" -c "$1" -o "$out/$2.o" "${@:3}"; }
build "$out/main-windows.c" main-windows -w
build "$cpu/cpu.c" cpu -Dgoose_init=fg143_cpu_init -Dgg_init_globals=fg143_cpu_globals
build "$src/graph-execute.c" graph-execute
build "$src/cuda-driver.c" cuda-driver
build "$src/legacy/graph-bindings.c" graph-bindings
build "$src/goose-host-llm.c" model-host
build "$src/windows/socket-forwards.c" libc-forwards
build "$src/windows/platform.c" platform
build "$src/windows/web.c" web
build "$src/slru-runtime-shim.c" slru-shim -DFG_SLRU_SHARED_UNIT
build "$root/integration/m1/host/goose_host.c" goose-host
build "$src/ggml-system.c" ggml-system
build "$src/cli/infer-main.c" infer-main -DFG_CHAT_ALPHA
build "$ggml/src/ggml.c" ggml
build "$ggml/src/ggml-quants.c" ggml-quants
build "$prob/probability.c" probability -Dgoose_init=fg_probability_init -Dgg_init_globals=fg_probability_globals
build "$prob/probability-pipeline.c" probability-pipeline -I "$prob"
"$cc" -municode -static -Wl,--gc-sections \
 "$out"/{infer-main,main-windows,cpu,graph-execute,cuda-driver,graph-bindings,model-host,libc-forwards,slru-shim,goose-host,ggml,ggml-quants,ggml-system,probability,probability-pipeline,platform,web}.o \
 -lws2_32 -lpsapi -lwinpthread -o "$out/gooseinfer-server.exe"
cp "$out/gooseinfer-server.exe" "$out/frankenmoe-server.exe"
"${WINDOWS_WINDRES:-x86_64-w64-mingw32-windres}" "$src/windows/launcher.rc" "$out/launcher-resources.o"
"$cc" "${flags[@]}" -static -municode "$src/windows/launcher.c" "$out/platform.o" "$out/launcher-resources.o" \
 -lcomdlg32 -lshell32 -lwinhttp -lws2_32 -lpsapi -ladvapi32 -o "$out/gooseinfer-launcher.exe"
cp "$out/gooseinfer-launcher.exe" "$out/frankenmoe-launcher.exe"
