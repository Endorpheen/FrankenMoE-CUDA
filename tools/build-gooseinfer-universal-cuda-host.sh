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
    linux) cc=${CC:-gcc-13} ;;
    windows) cc=${WINDOWS_CC:-x86_64-w64-mingw32-gcc} ;;
    *) printf 'Unknown GooseInfer target: %s\n' "$target" >&2; exit 2 ;;
esac
mkdir -p -- "$out"
flags=(-O2 -std=gnu11 -D_GNU_SOURCE -ffunction-sections -fdata-sections -Werror=implicit-function-declaration -I"$standalone")

"$cc" "${flags[@]}" -I"$images" -I"$cpu_headers" -c "$src/cuda-driver.c" -o "$out/cuda-driver.o"
"$cc" "${flags[@]}" -DI2_NUMERIC_MODULE -Dmain=gooseinfer_cuda_entry -I"$model_headers" \
    -I"$root/vendor/flint/ggml/include" -I"$root/integration/m1/host" \
    -c "$src/infer-main.c" -o "$out/cuda-entry.o"
