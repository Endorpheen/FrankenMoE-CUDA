#!/usr/bin/env bash
set -euo pipefail
source_dir=$(cd "$(dirname "$0")" && pwd)
repo=$(git rev-parse --show-toplevel)
out=${1:?Specify a new scratch directory}
previous="$repo/build/inkling-apex-gate/resident-bridge-1959"
link="$source_dir/../../EXP-2026-11-25-1959-inkling-resident-attention-moe/source/link.sh"
file=cuda-driver.reference.merged.c
mkdir "$out"
cp "$previous/"{graph-execute.c,inkling-backend.c,cuda-driver.c,infer-main.c,bridge.h,bridge-driver.h,bridge-image.h} "$out/"
patch -s "$previous/$file" -o "$out/$file" < "$source_dir/$file.patch"
cp "$link" "$out/link.sh"
bash "$out/link.sh"
