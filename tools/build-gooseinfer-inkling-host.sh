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
    linux) cc=${CC:-gcc-13}; objcopy=${OBJCOPY:-objcopy}; affinity_words=16 ;;
    windows) cc=${WINDOWS_CC:-x86_64-w64-mingw32-gcc}; objcopy=${WINDOWS_OBJCOPY:-x86_64-w64-mingw32-objcopy}; affinity_words=1 ;;
    *) printf 'Unknown GooseInfer target: %s\n' "$target" >&2; exit 2 ;;
esac
mkdir -p -- "$out"

for module in graph-execute inkling-backend; do
    object=$module
    if [[ "$module" == graph-execute ]]; then object=graph-native; fi
    "$cc" -O2 -std=gnu11 -D_GNU_SOURCE -DI2_NUMERIC_MODULE -DGOOSEINFER_TYPE_METADATA_ONLY -Werror=implicit-function-declaration \
        -ffunction-sections -fdata-sections "-fmacro-prefix-map=$root=." \
        -I"$source_dir/host" -I"$model_headers" -I"$cpu_headers" \
        -I"$root/vendor/flint/ggml/include" -I"$root/vendor/flint/ggml/src" \
        -I"$standalone" -I"$root/integration/m1/host" \
        -c "$source_dir/$module.c" -o "$out/$object.o"
done
printf 'const CPU_AFFINITY_WORDS: i64 = %s;\n' "$affinity_words" > "$out/graph-host.goose"
cat "$source_dir/graph-host.goose" >> "$out/graph-host.goose"
"${GOOSE_CUDA_DRIVER_BIN:?Set GOOSE_CUDA_DRIVER_BIN to Fried Goose KeelWing v0.3.1-dev.1}" \
    --standalone -o "$out/graph-host.c" "$out/graph-host.goose"
"$cc" -O2 -std=c11 -ffunction-sections -fdata-sections -Dgoose_init=fg_graph_host_init \
    -Dgg_init_globals=fg_graph_host_globals -c "$out/graph-host.c" -o "$out/graph-host.o"
"$objcopy" --redefine-sym=graph_host_strlen=strlen --redefine-sym=graph_host_perror=perror \
    --redefine-sym=graph_host_getaffinity=sched_getaffinity --redefine-sym=graph_host_setaffinity=sched_setaffinity \
    --redefine-sym=graph_host_getcpu=sched_getcpu "$out/graph-host.o"
"$cc" -r -nostdlib "$out/graph-native.o" "$out/graph-host.o" -o "$out/graph-execute.o"
