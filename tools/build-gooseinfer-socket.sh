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
standalone="$root/integration/goose-standalone"
source="$standalone/socket-$target.goose"
if [[ "$target" == windows ]]; then
    "$cc" -std=gnu11 -x c -fsyntax-only "$standalone/windows/platform-layout.h"
    source="${output%.o}.goose"
    cat "$standalone/socket-windows.goose" "$standalone/windows/platform.goose" "$standalone/windows/web.goose" > "$source"
fi
"${GOOSE_CUDA_DRIVER_BIN:?Set GOOSE_CUDA_DRIVER_BIN to Fried Goose KeelWing v0.3.1-dev.1}" \
    --standalone -o "${output%.o}.c" "$source"
"$cc" -O2 -std=gnu11 -ffunction-sections -fdata-sections \
    -Dgoose_init=fg_socket_init -Dgg_init_globals=fg_socket_globals -c "${output%.o}.c" -o "$output"
if [[ "$target" == windows ]]; then
    sed -nE 's/^extern "(fg_win_api_([[:alnum:]_]+))".*/\1 \2/p' "$source" > "${output%.o}.symbols"
    "${WINDOWS_OBJCOPY:-x86_64-w64-mingw32-objcopy}" --redefine-syms="${output%.o}.symbols" "$output"
fi
