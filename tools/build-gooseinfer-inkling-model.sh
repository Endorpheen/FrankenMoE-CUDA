#!/usr/bin/env bash
set -euo pipefail

target=${1:?Usage: build-gooseinfer-inkling-model.sh linux|windows OUTPUT_DIR [GENERATED_DIR]}
out=$(realpath -m -- "${2:?Missing output directory}")
root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
source_dir="$root/integration/goose-standalone/universal/inkling"
case "$target" in
    linux) cc=${CC:-gcc-13}; objcopy=${OBJCOPY:-objcopy} ;;
    windows) cc=${WINDOWS_CC:-x86_64-w64-mingw32-gcc}; objcopy=${WINDOWS_OBJCOPY:-x86_64-w64-mingw32-objcopy} ;;
    *) printf 'Unknown GooseInfer target: %s\n' "$target" >&2; exit 2 ;;
esac
mkdir -p -- "$out"

if [[ $# == 3 ]]; then
    generated=$(realpath -- "$3")
else
    generated=$out
    (cd -- "$root" && "${GOOSE_BIN:-goose}" --ast-budget-mib 2048 \
        --header "$out/main.h" -o "$out/main.c" "${source_dir#"$root"/}/model.goose")
fi

"$cc" -O1 -std=c11 -ffunction-sections -fdata-sections -w -c "$generated/main.c" -o "$out/inkling-model.o"
"$objcopy" --keep-global-symbols="$source_dir/model-exports.txt" "$out/inkling-model.o"
"$objcopy" --redefine-sym goose_init=inkling_model_init --redefine-sym ec_prepare=inkling_ec_prepare "$out/inkling-model.o"
