#!/usr/bin/env bash
set -euo pipefail

src=$(cd "$(dirname "$0")" && pwd)
root=$(git -C "$src" rev-parse --show-toplevel)
out=${1:?Specify the test build directory}
goose=${GOOSE_BIN:-/home/end0/AI/Fried-Goose/releases/fried-goose-v0.3.0-preview-keelwing/bin/goose}
mkdir -p "$out"
python3 "$root/tools/compose-goose.py" "$out/base.goose" \
    "$root/goose/model/qwen35-generation.goose" "$root/goose/sampling/sampler-chain-full.goose" "$src/fixture.goose"
python3 "$src/prepare-unit.py" "$out/base.goose" "$out/fixture.goose"
"$goose" --standalone -o "$out/generated.c" --header "$out/fixture.h" "$out/fixture.goose"
gcc-13 -O1 -std=c11 -ffunction-sections -fdata-sections -Wl,--gc-sections \
    -I"$out" "$out/generated.c" "$src/fixture.c" -lm -o "$out/test"
"$out/test"
