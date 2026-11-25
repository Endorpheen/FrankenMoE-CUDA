#!/usr/bin/env bash
set -euo pipefail
source_dir=$(cd "$(dirname "$0")" && pwd)
repo=$(git rev-parse --show-toplevel)
out=${1:?Specify a new scratch directory}
previous="$repo/build/inkling-apex-gate/resident-bridge-1959"
link="$source_dir/../../EXP-2026-11-25-1959-inkling-resident-attention-moe/source/link.sh"
mkdir "$out"
cp "$previous/"{graph-execute.c,cuda-driver.c,cuda-driver.reference.merged.c,infer-main.c,bridge-image.h} "$out/"
for f in inkling-backend.c bridge.h bridge-driver.h; do
    patch -s "$previous/$f" -o "$out/$f" < "$source_dir/$f.patch"
done
cp "$link" "$out/link.sh"
bash "$out/link.sh"
