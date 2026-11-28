#!/usr/bin/env bash
set -euo pipefail

target=${1:?Usage: build-gooseinfer-common-core.sh linux|windows OUTPUT_DIR MODEL_HEADERS}
out=$(realpath -m -- "${2:?Missing output directory}")
model_headers=$(realpath -- "${3:?Missing directory containing main.h}")
root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
standalone="$root/integration/goose-standalone"
ggml="$root/vendor/flint/ggml"
host="$root/integration/m1/host"
source_prefix=${GOOSEINFER_SOURCE_PREFIX:-.}
case "$target" in
    linux) cc=${CC:-gcc-13}; objcopy=${OBJCOPY:-objcopy}; hardening=(-D_FORTIFY_SOURCE=3 -fstack-protector-strong); native_long=i64; mutex=0 ;;
    windows) cc=${WINDOWS_CC:-x86_64-w64-mingw32-gcc}; objcopy=${WINDOWS_OBJCOPY:-x86_64-w64-mingw32-objcopy}; hardening=(); native_long=i32; mutex=0xffffffffffffffff ;;
    *) printf 'Unknown GooseInfer target: %s\n' "$target" >&2; exit 2 ;;
esac
mkdir -p -- "$out"

version=$(sed -nE 's/^set\(GGML_VERSION_(MAJOR|MINOR|PATCH) ([0-9]+)\)$/\2/p' "$ggml/CMakeLists.txt" | paste -sd.)
revision=${GGML_BUILD_COMMIT:-$(git -C "$root" rev-parse --short HEAD)}
sed -e "s/@GGML_VERSION@/$version/g" -e "s/@GGML_BUILD_COMMIT@/$revision/g" \
    "$ggml/src/ggml-version.h.in" > "$out/ggml-version.h"
printf '#include "main.h"\n' > "$out/goose-unit.h"
python3 "$root/tools/embed-gooseinfer-type-metadata.py" --goose \
    "$root/goose/misc/ggml-type-table.goose" "$root/goose/misc/ggml-op-names.goose" "$out/metadata.goose"

flags=(-std=c11 -D_GNU_SOURCE -ffunction-sections -fdata-sections
       -DGOOSEINFER_TYPE_METADATA_ONLY -DGOOSEINFER_GOOSE_METADATA -DGOOSEINFER_GOOSE_HOST_QUANT "-fmacro-prefix-map=$root=$source_prefix"
       -I"$out" -I"$model_headers" -I"$standalone" -I"$host"
       -I"$ggml/include" -I"$ggml/src" -I"$ggml/src/ggml-cpu")
"$cc" "${flags[@]}" -fsyntax-only "$ggml/src/ggml.c"
if [[ "$target" == linux ]]; then
    "$cc" -O1 "${flags[@]}" "${hardening[@]}" -c "$host/debugger-linux.c" -o "$out/debugger-linux.o"
fi
"$cc" "${flags[@]}" -x c -fsyntax-only "$standalone/graph-bindings-layout.h"
printf 'extern fn host_quant_lround(value: f32) -> %s;\n' "$native_long" > "$out/common-core.goose"
if "$cc" "${flags[@]}" -dM -E -x c /dev/null | awk '$2 == "NDEBUG" { seen = 1 } END { exit !seen }'; then
    printf 'const GGML_ARENA_DEBUG: bool = false;\n' >> "$out/common-core.goose"
else
    printf 'const GGML_ARENA_DEBUG: bool = true;\n' >> "$out/common-core.goose"
fi
printf 'const HOST_INIT_MUTEX: u64 = %s;\n' "$mutex" >> "$out/common-core.goose"
cat "$out/metadata.goose" "$standalone/"{graph-bindings,common-memory,graph-metadata,graph-arena-abi}.goose \
    "$host/"{host-quant,host-runtime,host-conversion,host-init}.goose "$host/host-io-$target.goose" >> "$out/common-core.goose"
"${GOOSE_CUDA_DRIVER_BIN:?Set GOOSE_CUDA_DRIVER_BIN to Fried Goose KeelWing v0.3.1-dev.1}" --standalone \
    -o "$out/graph-bindings.c" "$out/common-core.goose"
"$cc" -O2 "${flags[@]}" -Dgoose_init=fg_graph_bindings_init -Dgg_init_globals=fg_graph_bindings_globals \
    -c "$out/graph-bindings.c" -o "$out/graph-bindings.o"
if [[ "$target" == linux ]]; then
    io_symbols=(--redefine-sym=host_runtime_read=read --redefine-sym=host_runtime_write=write --redefine-sym=host_runtime_close=close
        --redefine-sym=host_posix_memalign=posix_memalign --redefine-sym=host_pool_free=free
        --redefine-sym=trace_open=fopen --redefine-sym=trace_line=fgets --redefine-sym=trace_close=fclose
        --redefine-sym=trace_pipe=pipe --redefine-sym=trace_pid=getpid --redefine-sym=trace_fork=fork
        --redefine-sym=trace_wait=waitpid --redefine-sym=trace_exec=execvp --redefine-sym=trace_exit=_Exit
        --redefine-sym=trace_frames=backtrace --redefine-sym=trace_symbols=backtrace_symbols_fd
        --redefine-sym=host_conv_time=time --redefine-sym=host_conv_localtime=localtime)
else
    io_symbols=(--redefine-sym=host_runtime_recv=recv --redefine-sym=host_runtime_send=send --redefine-sym=host_runtime_close=closesocket
        --redefine-sym=host_win_aligned_malloc=_aligned_malloc --redefine-sym=host_win_aligned_free=_aligned_free
        --redefine-sym=ggml_crt_write=_write --redefine-sym=host_conv_time=_time64 --redefine-sym=host_conv_localtime=_localtime64)
fi
"$objcopy" --redefine-sym=fg_host_model_initialize=goose_init --redefine-sym=ggml_flush_all=fflush --redefine-sym=ggml_diagnostic_strlen=strlen --redefine-sym=graph_crt_getenv=getenv --redefine-sym=host_quant_lround=lroundf \
    --redefine-sym=host_conv_strtod=strtod --redefine-sym=host_conv_strftime=strftime "${io_symbols[@]}" "$out/graph-bindings.o"
"$cc" -O1 "${flags[@]}" -c "$host/goose_host.c" -o "$out/goose-host.o"
CC="$cc" WINDOWS_CC="$cc" bash "$root/tools/build-gooseinfer-socket.sh" "$target" "$out/socket-forwards.o"
