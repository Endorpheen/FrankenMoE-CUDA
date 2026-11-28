#!/usr/bin/env bash
set -euo pipefail

target=${1:?Usage: build-gooseinfer-universal-cuda-host.sh linux|windows OUTPUT_DIR IMAGES CPU_HEADERS MODEL_HEADERS}
out=$(realpath -m -- "${2:?Missing output directory}")
images=$(realpath -- "${3:?Missing CUDA image headers directory}")
cpu_headers=$(realpath -- "${4:?Missing directory containing cpu.h}")
model_headers=$(realpath -- "${5:?Missing directory containing main.h}")
root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
standalone="$root/integration/goose-standalone"
src="$standalone/universal/cuda"
case "$target" in
    linux) cc=${CC:-gcc-13}; objcopy=${OBJCOPY:-objcopy} ;;
    windows) cc=${WINDOWS_CC:-x86_64-w64-mingw32-gcc}; objcopy=${WINDOWS_OBJCOPY:-x86_64-w64-mingw32-objcopy} ;;
    *) printf 'Unknown GooseInfer target: %s\n' "$target" >&2; exit 2 ;;
esac
mkdir -p -- "$out"
flags=(-O2 -std=gnu11 -D_GNU_SOURCE -ffunction-sections -fdata-sections)

bash "$root/tools/build-gooseinfer-cuda-driver.sh" "$target" "$out" "$images"
cat "$src/infer-main.goose" "$src/infer-platform-$target.goose" > "$out/cuda-entry.goose"
"${GOOSE_CUDA_DRIVER_BIN:?Set GOOSE_CUDA_DRIVER_BIN to Fried Goose KeelWing v0.3.1-dev.1}" \
    --standalone -o "$out/cuda-entry.c" "$out/cuda-entry.goose"
"$cc" "${flags[@]}" -Dgoose_init=gooseinfer_cuda_entry_init -Dgg_init_globals=gooseinfer_cuda_entry_globals \
    -c "$out/cuda-entry.c" -o "$out/cuda-entry.o"
"$objcopy" --redefine-sym=gooseinfer_cli_flush=fflush "$out/cuda-entry.o"
