#!/usr/bin/env bash
set -euo pipefail

target=${1:?Usage: build-gooseinfer-inkling-host.sh linux|windows OUTPUT_DIR MODEL_HEADERS CPU_HEADERS}
out=$(realpath -m -- "${2:?Missing output directory}")
model_headers=$(realpath -- "${3:?Missing directory containing main.h}")
cpu_headers=$(realpath -- "${4:?Missing directory containing cpu.h}")
root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
standalone="$root/integration/goose-standalone"
source_dir="$standalone/universal/inkling"
case "$target" in
    linux) cc=${CC:-gcc-13} ;;
    windows) cc=${WINDOWS_CC:-x86_64-w64-mingw32-gcc} ;;
    *) printf 'Unknown GooseInfer target: %s\n' "$target" >&2; exit 2 ;;
esac
mkdir -p -- "$out"

for module in graph-execute inkling-backend; do
    "$cc" -O2 -std=gnu11 -D_GNU_SOURCE -DI2_NUMERIC_MODULE -DGOOSEINFER_TYPE_METADATA_ONLY -Werror=implicit-function-declaration \
        -ffunction-sections -fdata-sections "-fmacro-prefix-map=$root=." \
        -I"$source_dir/host" -I"$model_headers" -I"$cpu_headers" \
        -I"$root/vendor/flint/ggml/include" -I"$root/vendor/flint/ggml/src" \
        -I"$standalone" -I"$root/integration/m1/host" \
        -c "$source_dir/$module.c" -o "$out/$module.o"
done
