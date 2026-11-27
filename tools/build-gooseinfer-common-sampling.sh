#!/usr/bin/env bash
set -euo pipefail

target=${1:?Usage: build-gooseinfer-common-sampling.sh linux|windows OUTPUT_DIR MODEL_HEADERS}
out=$(realpath -m -- "${2:?Missing output directory}")
model_headers=$(realpath -- "${3:?Missing directory containing main.h}")
root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
source_dir="$root/integration/goose-standalone/universal/common"
goose=${GOOSE_BIN:-goose}
case "$target" in
    linux) cc=${CC:-gcc-13}; objcopy=${OBJCOPY:-objcopy}; pic=(-fPIC) ;;
    windows) cc=${WINDOWS_CC:-x86_64-w64-mingw32-gcc}; objcopy=${WINDOWS_OBJCOPY:-x86_64-w64-mingw32-objcopy}; pic=() ;;
    *) printf 'Unknown GooseInfer target: %s\n' "$target" >&2; exit 2 ;;
esac
mkdir -p -- "$out"
flags=(-std=c11 -ffunction-sections -fdata-sections)

cat "$source_dir/probability.goose" "$source_dir/probability-pipeline.goose" "$source_dir/sampler-scan.goose" > "$out/probability.goose"
"${GOOSE_CUDA_DRIVER_BIN:?Set GOOSE_CUDA_DRIVER_BIN to Fried Goose KeelWing v0.3.1-dev.1}" \
    --standalone -o "$out/probability.c" --header "$out/probability.h" "$out/probability.goose"
"$goose" --standalone -o "$out/planner.c" --header "$out/planner.h" "$source_dir/planner.goose"
"$cc" -O3 "${flags[@]}" -w -Dgoose_init=fg_probability_init -Dgg_init_globals=fg_probability_globals \
    -c "$out/probability.c" -o "$out/probability.o"
"$cc" -O1 "${flags[@]}" -I"$out" -c "$source_dir/probability-pipeline.c" -o "$out/probability-pipeline.o"
"$cc" -O3 "${pic[@]}" "${flags[@]}" -w -c "$out/planner.c" -o "$out/planner.o"
"$objcopy" --redefine-sym ec_prepare=exp1744_prepare_unused --redefine-sym ec_prepare_compact=exp1744_compact_unused \
    --redefine-sym ec_reset=exp1744_reset_unused --redefine-sym gg_init_globals=exp1744_init_globals \
    --redefine-sym goose_init=exp1744_init --redefine-sym slru_plan=exp1744_slru_unused \
    "$out/planner.o" "$out/planner-link.o"
