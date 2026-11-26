#!/usr/bin/env bash
set -euo pipefail

src=$(cd "$(dirname "$0")" && pwd)
root=$(git -C "$src" rev-parse --show-toplevel)
out=${1:?Specify the test build directory}
logits=${2:?Specify saved EXP-2096 full logits}
build=${FRANKENMOE_CHECKPOINTS:-$root/build}
goose=${GOOSE_BIN:-/home/end0/AI/Fried-Goose/releases/fried-goose-v0.3.0-preview-keelwing/bin/goose}
mkdir -p "$out"
ar p "$build/bonsai-dp4a-2012-default/libfranken-inference.a" sampler-scan.o > "$out/sampler-scan.o"
printf 'c9f7d49e104394e1f844a75e1c4c6830acb9a05565ecb936d1335ca6eed46217  %s\n' "$out/sampler-scan.o" | sha256sum --strict -c - >&2
python3 "$root/tools/compose-goose.py" "$out/base.goose" \
    "$root/goose/sampling/sampler-chain-full.goose" "$src/fixture.goose" >&2
python3 "$src/prepare-greedy.py" "$out/base.goose" "$out/fixture.goose"
"$goose" --standalone -o "$out/generated.c" --header "$out/fixture.h" "$out/fixture.goose"
gcc-13 -O1 -std=c11 -ffunction-sections -fdata-sections -Wl,--gc-sections \
    -I"$out" "$out/generated.c" "$src/fixture.c" "$out/sampler-scan.o" -lm -o "$out/test"
taskset 0f0f "$out/test" "$logits"
