#!/usr/bin/env bash
set -euo pipefail

target=${1:?Usage: build-gooseinfer-inkling-primitives.sh linux|windows OUTPUT_DIR}
out=$(realpath -m -- "${2:?Missing output directory}")
root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
source_dir="$root/integration/goose-standalone/universal/inkling"
goose=${GOOSE_BIN:-goose}
case "$target" in
    linux) cc=${CC:-gcc-13}; objcopy=${OBJCOPY:-objcopy} ;;
    windows) cc=${WINDOWS_CC:-x86_64-w64-mingw32-gcc}; objcopy=${WINDOWS_OBJCOPY:-x86_64-w64-mingw32-objcopy} ;;
    *) printf 'Unknown GooseInfer target: %s\n' "$target" >&2; exit 2 ;;
esac
mkdir -p -- "$out"

for module in numeric lfu prior bucket; do
    namespace=i2_candidate
    if [[ $module != numeric ]]; then namespace=i2_$module; fi
    "$goose" --standalone -o "$out/$module.c" --header "$out/$module.h" "$source_dir/$module.goose"
    "$cc" -O1 -std=c11 -ffunction-sections -fdata-sections -w \
        "-Dgoose_init=${namespace}_init" "-Dgg_init_globals=${namespace}_globals" \
        -c "$out/$module.c" -o "$out/$module.o"
done

"$goose" --standalone -o "$out/tokenizer.c" --header "$out/tokenizer.h" "$source_dir/tokenizer.goose"
"$cc" -O2 -std=c11 -ffunction-sections -fdata-sections -w \
    -Dgoose_init=inkling_tokenizer_init -Dgg_init_globals=inkling_tokenizer_globals \
    -c "$out/tokenizer.c" -o "$out/tokenizer-unlinked.o"
"$objcopy" --keep-global-symbol=inkling_tokenizer_init --keep-global-symbol=bt_tokenize_inkling2 \
    "$out/tokenizer-unlinked.o" "$out/tokenizer.o"

if [[ $target == windows ]]; then
    model_headers=$(realpath -- "${3:?Missing directory containing main.h}")
    "$cc" -O3 -std=c11 -ffunction-sections -fdata-sections -I"$model_headers" \
        -c "$source_dir/sampler-scan.c" -o "$out/inkling-sampler-scan.o"
fi
