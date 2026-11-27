#!/usr/bin/env bash
set -euo pipefail

target=${1:?Usage: build-gooseinfer-cuda-driver.sh linux|windows OUTPUT_DIR CUDA_IMAGE_HEADERS}
out=$(realpath -m -- "${2:?Missing output directory}")
images=$(realpath -- "${3:?Missing CUDA image headers directory}")
root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
standalone="$root/integration/goose-standalone"
source_dir="$standalone/universal/cuda"
goose=${GOOSE_CUDA_DRIVER_BIN:-${GOOSE_BIN:?Set GOOSE_CUDA_DRIVER_BIN to Fried Goose KeelWing v0.3.1-dev.1}}
case "$target" in
    linux) cc=${CC:-gcc-13} ;;
    windows) cc=${WINDOWS_CC:-x86_64-w64-mingw32-gcc} ;;
    *) printf 'Unknown GooseInfer target: %s\n' "$target" >&2; exit 2 ;;
esac
mkdir -p -- "$out"
generate=(python3 "$root/tools/emit-gooseinfer-cuda-driver-abi.py"
    "$source_dir/driver-abi.txt" "$out/driver-abi.goose" "$out/driver-abi.S" --native-target "$target")
"${generate[@]}"
cat "$out/driver-abi.goose" "$source_dir/driver-replay.goose" "$source_dir/driver.goose" > "$out/driver.goose"
"$goose" --standalone -o "$out/driver.c" --header "$out/driver.h" "$out/driver.goose"
"${generate[@]}" --tls-source "$out/driver.c" --tls-output "$out/driver-tls.c" \
    --image-output "$out/driver-host.c" \
    --diagnostics "$source_dir/driver-diagnostics.json" --diagnostics-output "$out/driver-diagnostics.c"

flags=(-O1 -std=gnu11 -D_GNU_SOURCE -ffunction-sections -fdata-sections -Werror=implicit-function-declaration)
"$cc" "${flags[@]}" -Dgoose_init=goose_cuda_init -Dgg_init_globals=goose_cuda_globals \
    -c "$out/driver-tls.c" -o "$out/driver-tls.o"
"$cc" "${flags[@]}" -I"$images" -I"$standalone" -c "$out/driver-host.c" -o "$out/driver-host.o"
"$cc" "${flags[@]}" -c "$out/driver-diagnostics.c" -o "$out/driver-diagnostics.o"
"$cc" -c "$out/driver-abi.S" -o "$out/driver-abi.o"
"$cc" -r "$out/"{driver-tls,driver-host,driver-diagnostics,driver-abi}.o -o "$out/cuda-driver.o"
