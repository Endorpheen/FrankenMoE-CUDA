#!/usr/bin/env bash
set -euo pipefail
source_dir=$(cd "$(dirname "$0")" && pwd)
repo=$(git rev-parse --show-toplevel)
out=${1:?Specify a new scratch directory}
previous="$repo/build/inkling-apex-gate/decode-lfu-1962"
host="$repo/build/inkling-exp-baseline/device-attention/inkling-pinned-activation-readback"
mkdir "$out"
cp "$previous/"{graph-execute.c,cuda-driver.c,cuda-driver.reference.merged.c,infer-main.c,bridge.h,bridge-driver.h,bridge-image.h,decode-lfu.h,lfu.goose,link.sh} "$out/"
patch -s "$previous/inkling-backend.c" -o "$out/inkling-backend.c" < "$source_dir/inkling-backend.c.patch"
for f in prefill.h prefill-team.h; do
    patch -s "$host/$f" -o "$out/$f" < "$source_dir/$f.patch"
done
bash "$out/link.sh"
