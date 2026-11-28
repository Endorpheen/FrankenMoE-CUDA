#!/usr/bin/env bash
set -euo pipefail

target=${1:?Specify linux or windows}
output=${2:?Specify the output object path}
root=$(cd "$(dirname "$0")/.." && pwd)
case "$target" in
    linux) cc=${CC:-gcc-13} ;;
    windows) cc=${WINDOWS_CC:-x86_64-w64-mingw32-gcc} ;;
    *) exit 2 ;;
esac
"${GOOSE_CUDA_DRIVER_BIN:?Set GOOSE_CUDA_DRIVER_BIN to Fried Goose KeelWing v0.3.1-dev.1}" \
    --standalone -o "${output%.o}.c" "$root/integration/goose-standalone/socket-$target.goose"
"$cc" -O2 -std=gnu11 -ffunction-sections -fdata-sections \
    -Dgoose_init=fg_socket_init -Dgg_init_globals=fg_socket_globals -c "${output%.o}.c" -o "$output"
