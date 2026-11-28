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
    linux) cc=${CC:-gcc-13}; objcopy=${OBJCOPY:-objcopy}; affinity_words=16; mutex=0 ;;
    windows) cc=${WINDOWS_CC:-x86_64-w64-mingw32-gcc}; objcopy=${WINDOWS_OBJCOPY:-x86_64-w64-mingw32-objcopy}; affinity_words=1; mutex=0xffffffffffffffff ;;
    *) printf 'Unknown GooseInfer target: %s\n' "$target" >&2; exit 2 ;;
esac
mkdir -p -- "$out"

native_flags=(-std=gnu11 -D_GNU_SOURCE -DI2_NUMERIC_MODULE -DGOOSEINFER_TYPE_METADATA_ONLY
    -Werror=implicit-function-declaration -ffunction-sections -fdata-sections "-fmacro-prefix-map=$root=."
    -I"$source_dir/host" -I"$model_headers" -I"$cpu_headers"
    -I"$root/vendor/flint/ggml/include" -I"$root/vendor/flint/ggml/src" -I"$standalone" -I"$root/integration/m1/host")
"$cc" "${native_flags[@]}" -fsyntax-only "$source_dir/graph-platform.c"
"$cc" "${native_flags[@]}" -fsyntax-only "$source_dir/inkling-backend.c"
printf 'const CPU_AFFINITY_WORDS: i64 = %s;\n' "$affinity_words" > "$out/graph-host.goose"
printf 'const GRAPH_INIT_MUTEX: u64 = %s;\n' "$mutex" >> "$out/graph-host.goose"
cat "$source_dir/"{graph-host,graph-state,graph-diagnostics}.goose "$source_dir/graph-checkpoint-$target.goose" \
    "$source_dir/graph-storage-$target.goose" >> "$out/graph-host.goose"
"${GOOSE_CUDA_DRIVER_BIN:?Set GOOSE_CUDA_DRIVER_BIN to Fried Goose KeelWing v0.3.1-dev.1}" \
    --standalone -o "$out/graph-host.c" "$out/graph-host.goose"
"$cc" -O2 -std=c11 -ffunction-sections -fdata-sections -Dgoose_init=fg_graph_host_init \
    -Dgg_init_globals=fg_graph_host_globals -c "$out/graph-host.c" -o "$out/graph-execute.o"
"$objcopy" --redefine-sym=graph_state_mutex_lock=pthread_mutex_lock --redefine-sym=graph_state_mutex_unlock=pthread_mutex_unlock \
    --redefine-sym=graph_host_strlen=strlen --redefine-sym=graph_host_perror=perror \
    --redefine-sym=graph_host_getaffinity=sched_getaffinity --redefine-sym=graph_host_setaffinity=sched_setaffinity \
    --redefine-sym=graph_host_getcpu=sched_getcpu "$out/graph-execute.o"
if [[ "$target" == linux ]]; then
    "$objcopy" --redefine-sym=graph_ckpt_unix_open=open --redefine-sym=graph_ckpt_unix_openat=openat \
        --redefine-sym=graph_ckpt_fdopen=fdopen --redefine-sym=graph_ckpt_close=close \
        --redefine-sym=graph_ckpt_fwrite=fwrite \
        --redefine-sym=graph_storage_fopen=fopen --redefine-sym=graph_storage_fgets=fgets \
        --redefine-sym=graph_storage_fclose=fclose "$out/graph-execute.o"
fi
cat "$source_dir/"{draft-host,open-host,control-host,graph-nodes-host,joined-host,profile-host,serial-host,team-host,state-diagnostics,graph-eval-host}.goose "$source_dir/team-layout-$target.goose" > "$out/draft-host.goose"
"${GOOSE_CUDA_DRIVER_BIN:?}" --standalone -o "$out/draft-host.c" "$out/draft-host.goose"
"$cc" -O2 -std=c11 -ffunction-sections -fdata-sections -Dgoose_init=fg_i2_backend_host_init \
    -Dgg_init_globals=fg_i2_backend_host_globals -c "$out/draft-host.c" -o "$out/inkling-backend.o"
for name in mutex_init mutex_destroy mutex_lock mutex_unlock cond_init cond_destroy cond_wait cond_signal cond_broadcast; do
    "$objcopy" --redefine-sym="i2_team_$name=pthread_$name" "$out/inkling-backend.o"
done
