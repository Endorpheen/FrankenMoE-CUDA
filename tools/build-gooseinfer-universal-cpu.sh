#!/usr/bin/env bash
set -euo pipefail

target=${1:?Usage: build-gooseinfer-universal-cpu.sh linux|windows OUTPUT_DIR}
out=$(realpath -m -- "${2:?Missing output directory}")
root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
source_dir="$root/integration/goose-standalone/universal/cpu"
goose=${GOOSE_BIN:-goose}
case "$target" in
    linux) cc=${CC:-gcc-13}; objcopy=${OBJCOPY:-objcopy} ;;
    windows) cc=${WINDOWS_CC:-x86_64-w64-mingw32-gcc}; objcopy=${WINDOWS_OBJCOPY:-x86_64-w64-mingw32-objcopy} ;;
    *) printf 'Unknown GooseInfer target: %s\n' "$target" >&2; exit 2 ;;
esac
mkdir -p -- "$out"

"$goose" --standalone -o "$out/cpu.c" --header "$out/cpu.h" "$source_dir/cpu.goose"
"$cc" -O1 -std=c11 -ffunction-sections -fdata-sections -Wno-discarded-qualifiers \
    -Dgoose_init=goose_cpu_init -Dgg_init_globals=goose_cpu_globals -c "$out/cpu.c" -o "$out/cpu-unlinked.o"
"$objcopy" --redefine-sym goose_cpu_init=fg143_cpu_init --redefine-sym goose_cpu_globals=fg143_cpu_init_globals \
    "$out/cpu-unlinked.o" "$out/cpu.o"
