#!/usr/bin/env bash
set -euo pipefail

target=${1:?Usage: build-gooseinfer-bonsai-session.sh linux|windows OUTPUT_DIR MODEL_HEADERS}
out=$(realpath -m -- "${2:?Missing output directory}")
model_headers=$(realpath -- "${3:?Missing directory containing main.h}")
root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
standalone="$root/integration/goose-standalone"
src="$standalone/universal/bonsai"
cuda=${CUDA_HOME:-/usr/local/cuda}
case "$target" in
    linux) cc=${CC:-gcc-13}; objcopy=${OBJCOPY:-objcopy} ;;
    windows) cc=${WINDOWS_CC:-x86_64-w64-mingw32-gcc}; objcopy=${WINDOWS_OBJCOPY:-x86_64-w64-mingw32-objcopy} ;;
    *) printf 'Unknown GooseInfer target: %s\n' "$target" >&2; exit 2 ;;
esac
mkdir -p -- "$out"

"$cc" -O2 -Wall -Wextra -Werror -ffunction-sections -fdata-sections \
    -I"$standalone" -I"$cuda/include" \
    -fsyntax-only "$src/backend-cold.c"

ggml="$root/vendor/flint/ggml"
"$cc" -O2 -std=gnu11 -D_GNU_SOURCE -DI2_NUMERIC_MODULE -Werror=implicit-function-declaration \
    -ffunction-sections -fdata-sections -I"$model_headers" -I"$standalone" \
    -I"$standalone/universal/inkling" -I"$standalone/gigachat4" -I"$root/integration/m1/host" \
    -I"$ggml/include" -I"$ggml/src" -fsyntax-only "$src/model-host.c"
if [[ "$target" == linux ]]; then
    has_populate=$("$cc" -D_GNU_SOURCE -include sys/mman.h -dM -E -x c /dev/null |
        awk '$2 == "MADV_POPULATE_READ" { found = 1 } END { print found ? "true" : "false" }')
    printf 'const MODEL_HAS_POPULATE_READ: bool = %s;\n' "$has_populate" > "$out/host-vocab.goose"
    printf 'const BONSAI_MUTEX_INITIAL: u64 = 0;\n' >> "$out/host-vocab.goose"
else
    printf 'const BONSAI_MUTEX_INITIAL: u64 = 0xffffffffffffffff;\n' > "$out/host-vocab.goose"
fi
cat "$src/"{host-vocab,driver-host,batch-host,kernel-launch-host,verify-host,matrix-host,ffn-graph-host,session-host,draft-host,workspace-host,image-host,profile-host,model-open-host,mtp-session,host-lifecycle,host-generation}.goose "$src/host-map-$target.goose" >> "$out/host-vocab.goose"
"${GOOSE_CUDA_DRIVER_BIN:?Set GOOSE_CUDA_DRIVER_BIN to Fried Goose KeelWing v0.3.1-dev.1}" \
    --standalone -o "$out/host-vocab.c" "$out/host-vocab.goose"
"$cc" -O2 -std=c11 -ffunction-sections -fdata-sections -Dgoose_init=fg_model_vocab_init \
    -Dgg_init_globals=fg_model_vocab_globals -c "$out/host-vocab.c" -o "$out/model-host.o"
native=(--redefine-sym=vocab_decode_setspecific=pthread_setspecific --redefine-sym=vocab_touch_copy=memcpy --redefine-sym=vocab_strlen=strlen --redefine-sym=fg_cuda_model_getenv=getenv
    --redefine-sym=bonsai_mutex_lock=pthread_mutex_lock --redefine-sym=bonsai_mutex_unlock=pthread_mutex_unlock)
if [[ "$target" == linux ]]; then
    native+=(--redefine-sym=vocab_clock_gettime=clock_gettime --redefine-sym=vocab_unix_open=open --redefine-sym=vocab_unix_fstat=fstat
        --redefine-sym=vocab_unix_sysconf=sysconf --redefine-sym=vocab_unix_mmap=mmap --redefine-sym=vocab_unix_close=close
        --redefine-sym=bonsai_munmap=munmap --redefine-sym=bonsai_perror=perror
        --redefine-sym=vocab_unix_mincore=mincore --redefine-sym=vocab_unix_madvise=madvise --redefine-sym=vocab_unix_errno=__errno_location)
else
    native+=(--redefine-sym=vocab_clock_gettime=clock_gettime64 --redefine-sym=bonsai_win_unmap=UnmapViewOfFile)
fi
"$objcopy" "${native[@]}" "$out/model-host.o"
