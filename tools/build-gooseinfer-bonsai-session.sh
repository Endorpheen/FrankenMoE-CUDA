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
    -I"$standalone" -I"$cuda/include" -Dmain=bonsai_diagnostic_main \
    -c "$src/backend-cold.c" -o "$out/session-unlinked.o"
"$objcopy" --redefine-syms="$src/session-symbols.txt" "$out/session-unlinked.o" "$out/bonsai-session.o"

ggml="$root/vendor/flint/ggml"
"$cc" -O2 -std=gnu11 -D_GNU_SOURCE -DI2_NUMERIC_MODULE -Werror=implicit-function-declaration \
    -ffunction-sections -fdata-sections -I"$model_headers" -I"$standalone" \
    -I"$standalone/universal/inkling" -I"$standalone/gigachat4" -I"$root/integration/m1/host" \
    -I"$ggml/include" -I"$ggml/src" -c "$src/model-host.c" -o "$out/model-host.o"
