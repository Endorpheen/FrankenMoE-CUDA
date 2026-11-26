#!/usr/bin/env bash
set -euo pipefail

src=$(cd "$(dirname "$0")" && pwd)
out=${1:?Specify the isolated build directory}
base=${2:?Specify the recovered EXP-1728 main.goose}
goose=${GOOSE_BIN:-/home/end0/AI/Fried-Goose/releases/fried-goose-v0.3.0-preview-keelwing/bin/goose}
mkdir -p "$out"

printf '291a3d4f9c2562c79d6ea9c3477f567d6cc0f105aa5ceb91c4c147badf31e06f  %s\n' "$base" | sha256sum --strict -c -
python3 "$src/prepare-unit.py" "$base" "$out/main-choice.goose"
"$goose" --ast-budget-mib 2048 --standalone -o "$out/main.c" --header "$out/main.h" "$out/main-choice.goose"
gcc-13 -O1 -std=c11 -ffunction-sections -fdata-sections -w -c "$out/main.c" -o "$out/main.o"
