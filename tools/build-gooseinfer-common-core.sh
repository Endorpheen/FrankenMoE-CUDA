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
    linux) cc=${CC:-gcc-13}; hardening=(-D_FORTIFY_SOURCE=3 -fstack-protector-strong) ;;
    windows) cc=${WINDOWS_CC:-x86_64-w64-mingw32-gcc}; hardening=() ;;
    *) printf 'Unknown GooseInfer target: %s\n' "$target" >&2; exit 2 ;;
esac
mkdir -p -- "$out"

version=$(sed -nE 's/^set\(GGML_VERSION_(MAJOR|MINOR|PATCH) ([0-9]+)\)$/\2/p' "$ggml/CMakeLists.txt" | paste -sd.)
revision=${GGML_BUILD_COMMIT:-$(git -C "$root" rev-parse --short HEAD)}
sed -e "s/@GGML_VERSION@/$version/g" -e "s/@GGML_BUILD_COMMIT@/$revision/g" \
    "$ggml/src/ggml-version.h.in" > "$out/ggml-version.h"
printf '#include "main.h"\n' > "$out/goose-unit.h"

flags=(-std=c11 -D_GNU_SOURCE -ffunction-sections -fdata-sections
       "-fmacro-prefix-map=$root=$source_prefix"
       -I"$out" -I"$model_headers" -I"$standalone" -I"$host"
       -I"$ggml/include" -I"$ggml/src" -I"$ggml/src/ggml-cpu")
for unit in ggml ggml-quants; do
    "$cc" -O1 "${flags[@]}" "${hardening[@]}" -c "$ggml/src/$unit.c" -o "$out/$unit.o"
done
for unit in ggml-system graph-bindings; do
    "$cc" -O1 "${flags[@]}" "${hardening[@]}" -c "$standalone/$unit.c" -o "$out/$unit.o"
done
"$cc" -O1 "${flags[@]}" -c "$host/goose_host.c" -o "$out/goose-host.o"
"$cc" -O1 "${flags[@]}" -DFG_SLRU_SHARED_UNIT -c "$standalone/slru-runtime-shim.c" -o "$out/slru-shim.o"
if [[ $target == linux ]]; then
    "$cc" -O1 "${flags[@]}" -c "$standalone/libc-forwards.c" -o "$out/libc-forwards.o"
else
    for unit in platform socket-forwards web; do
        "$cc" -O2 "${flags[@]}" -c "$standalone/windows/$unit.c" -o "$out/$unit.o"
    done
fi
