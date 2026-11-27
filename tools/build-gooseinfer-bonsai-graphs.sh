#!/usr/bin/env bash
set -euo pipefail

target=${1:?Usage: build-gooseinfer-bonsai-graphs.sh linux|windows OUTPUT_DIR}
out=$(realpath -m -- "${2:?Missing output directory}")
root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
src=integration/goose-standalone/universal/bonsai
goose=${GOOSE_BIN:-goose}
case "$target" in
    linux) cc=${CC:-gcc-13}; objcopy=${OBJCOPY:-objcopy}; pic=(-fPIC) ;;
    windows) cc=${WINDOWS_CC:-x86_64-w64-mingw32-gcc}; objcopy=${WINDOWS_OBJCOPY:-x86_64-w64-mingw32-objcopy}; pic=() ;;
    *) printf 'Unknown GooseInfer target: %s\n' "$target" >&2; exit 2 ;;
esac
mkdir -p -- "$out"
cd -- "$root"
flags=(-O2 "${pic[@]}" -ffunction-sections -fdata-sections -w)

cat "$src/ffn-tail.goose" "$src/session-plans.goose" > "$out/ffn-tail.goose"
for module in prefix ffn-tail; do
    source="$src/$module.goose"
    compiler="$goose"
    if [[ $module == ffn-tail ]]; then
        source="$out/ffn-tail.goose"
        compiler=${GOOSE_CUDA_DRIVER_BIN:?Set GOOSE_CUDA_DRIVER_BIN to Fried Goose KeelWing v0.3.1-dev.1}
    fi
    "$compiler" --standalone -o "$out/$module.c" "$source"
    "$cc" "${flags[@]}" -c "$out/$module.c" -o "$out/$module.o"
done
awk '{print $1}' "$src/session-symbols.txt" > "$out/exports.txt"
"$objcopy" --keep-global-symbols="$out/exports.txt" "$out/ffn-tail.o" "$out/bonsai-goose.o"
"$objcopy" --redefine-syms="$src/session-symbols.txt" "$out/bonsai-goose.o"
"$objcopy" --redefine-sym bonsai_graph=bonsai_mtp_target_graph --redefine-sym goose_init=bonsai_mtp_goose_init \
    "$out/prefix.o" "$out/mtp-goose.o"
"$objcopy" --keep-global-symbol=bonsai_mtp_target_graph --keep-global-symbol=bonsai_mtp_head \
    --keep-global-symbol=bonsai_mtp_goose_init --keep-global-symbol=bonsai_mtp_kv \
    --keep-global-symbol=bonsai_mtp_kv_batch "$out/mtp-goose.o"
