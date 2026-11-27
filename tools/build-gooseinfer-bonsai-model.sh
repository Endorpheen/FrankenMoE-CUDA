#!/usr/bin/env bash
set -euo pipefail

target=${1:?Usage: build-gooseinfer-bonsai-model.sh linux|windows OUTPUT_DIR}
out=$(realpath -m -- "${2:?Missing output directory}")
root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
source=integration/goose-standalone/universal/bonsai/main-mtp.goose
case "$target" in
    linux) cc=${CC:-gcc-13} ;;
    windows) cc=${WINDOWS_CC:-x86_64-w64-mingw32-gcc} ;;
    *) printf 'Unknown GooseInfer target: %s\n' "$target" >&2; exit 2 ;;
esac
mkdir -p -- "$out"
goose=${GOOSE_HOST_BIN:?Set GOOSE_HOST_BIN to Fried Goose KeelWing ownership v2}
stdlib=$(realpath -- "${GOOSE_HOST_STDLIB:?Set GOOSE_HOST_STDLIB to the matching KeelWing stdlib directory}")
cp -- "$root/$source" "$out/model.goose"
ln -sfn -- "$stdlib/math.goose" "$out/math.goose"
ln -sfn -- "$stdlib/std.goose" "$out/std.goose"
(
    cd -- "$out"
    ulimit -s 65536
    "$goose" --ast-budget-mib 2048 --header "$out/main.h" -o "$out/main.c" model.goose
)
"$cc" -O1 -std=c11 -ffunction-sections -fdata-sections -w -c "$out/main.c" -o "$out/main.o"
