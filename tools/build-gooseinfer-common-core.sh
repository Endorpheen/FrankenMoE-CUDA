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
    linux) cc=${CC:-gcc-13}; objcopy=${OBJCOPY:-objcopy}; hardening=(-D_FORTIFY_SOURCE=3 -fstack-protector-strong) ;;
    windows) cc=${WINDOWS_CC:-x86_64-w64-mingw32-gcc}; objcopy=${WINDOWS_OBJCOPY:-x86_64-w64-mingw32-objcopy}; hardening=() ;;
    *) printf 'Unknown GooseInfer target: %s\n' "$target" >&2; exit 2 ;;
esac
mkdir -p -- "$out"

version=$(sed -nE 's/^set\(GGML_VERSION_(MAJOR|MINOR|PATCH) ([0-9]+)\)$/\2/p' "$ggml/CMakeLists.txt" | paste -sd.)
revision=${GGML_BUILD_COMMIT:-$(git -C "$root" rev-parse --short HEAD)}
sed -e "s/@GGML_VERSION@/$version/g" -e "s/@GGML_BUILD_COMMIT@/$revision/g" \
    "$ggml/src/ggml-version.h.in" > "$out/ggml-version.h"
printf '#include "main.h"\n' > "$out/goose-unit.h"
python3 "$root/tools/embed-gooseinfer-type-metadata.py" \
    "$root/goose/misc/ggml-type-table.goose" "$out/gooseinfer-type-metadata.h" \
    "$root/goose/misc/ggml-op-names.goose" "$out/gooseinfer-op-names.h"

flags=(-std=c11 -D_GNU_SOURCE -ffunction-sections -fdata-sections
       -DGOOSEINFER_TYPE_METADATA_ONLY "-fmacro-prefix-map=$root=$source_prefix"
       -I"$out" -I"$model_headers" -I"$standalone" -I"$host"
       -I"$ggml/include" -I"$ggml/src" -I"$ggml/src/ggml-cpu")
"$cc" -O1 "${flags[@]}" "${hardening[@]}" \
    -c "$ggml/src/ggml.c" -o "$out/ggml.o"
"$cc" "${flags[@]}" -x c -fsyntax-only "$standalone/graph-bindings-layout.h"
cat "$standalone/graph-bindings.goose" "$standalone/common-memory.goose" > "$out/common-core.goose"
"${GOOSE_CUDA_DRIVER_BIN:?Set GOOSE_CUDA_DRIVER_BIN to Fried Goose KeelWing v0.3.1-dev.1}" --standalone \
    -o "$out/graph-bindings.c" "$out/common-core.goose"
"$cc" -O2 "${flags[@]}" -Dgoose_init=fg_graph_bindings_init -Dgg_init_globals=fg_graph_bindings_globals \
    -c "$out/graph-bindings.c" -o "$out/graph-bindings.o"
"$objcopy" --redefine-sym=graph_crt_getenv=getenv "$out/graph-bindings.o"
"$cc" -O1 "${flags[@]}" -c "$host/goose_host.c" -o "$out/goose-host.o"
CC="$cc" WINDOWS_CC="$cc" bash "$root/tools/build-gooseinfer-socket.sh" "$target" "$out/socket-forwards.o"
if [[ $target == windows ]]; then
    "$cc" -O2 "${flags[@]}" -c "$standalone/windows/web.c" -o "$out/web.o"
fi
