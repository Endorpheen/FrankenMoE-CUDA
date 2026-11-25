#!/usr/bin/env bash
set -euo pipefail
source_dir=$(cd "$(dirname "$0")" && pwd)
repo=$(git rev-parse --show-toplevel)
out=${1:?Specify a new scratch directory}
previous="$repo/build/inkling-apex-gate/expert-trace-1961"
file=cuda-driver.reference.merged.c
mkdir "$out"
cp "$previous/"{graph-execute.c,inkling-backend.c,cuda-driver.c,infer-main.c,bridge.h,bridge-driver.h,bridge-image.h} "$out/"
cp "$source_dir/"{decode-lfu.h,lfu.goose,link.sh} "$out/"
patch -s "$previous/$file" -o "$out/$file" < "$source_dir/$file.patch"
bash "$out/link.sh"
