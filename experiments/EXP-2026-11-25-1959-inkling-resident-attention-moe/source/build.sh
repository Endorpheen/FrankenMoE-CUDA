#!/usr/bin/env bash
set -euo pipefail
source_dir=$(cd "$(dirname "$0")" && pwd)
repo=$(git rev-parse --show-toplevel)
out=${1:?Specify a new scratch directory}
previous="$repo/build/inkling-apex-gate/single-link-1953"
image="$repo/build/inkling-apex-gate/attention-expf-1958/attention-bridge.cubin"
inkling="$repo/build/inkling-exp-baseline/device-attention/inkling-pinned-activation-readback"
mkdir "$out"
out=$(realpath "$out")
cp "$previous/"{cuda-driver.reference.merged.c,infer-main.c} "$out/"
cp "$source_dir/"{bridge.h,bridge-driver.h} "$out/"
cp "$image" "$out/attention-bridge.cubin"
(cd "$out" && xxd -i attention-bridge.cubin > bridge-image.h)
for f in graph-execute.c cuda-driver.c; do
    patch -s "$previous/$f" -o "$out/$f" < "$source_dir/$f.patch"
done
patch -s "$inkling/inkling-backend.c" -o "$out/inkling-backend.c" < "$source_dir/inkling-backend.c.patch"
cp "$source_dir/link.sh" "$out/"
bash "$out/link.sh"
