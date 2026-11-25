#!/usr/bin/env bash
set -euo pipefail
source_dir=$(cd "$(dirname "$0")" && pwd)
repo=$(git rev-parse --show-toplevel)
out=${1:?Specify a new scratch directory}
previous="$repo/build/inkling-apex-gate/parallel-logits-1966"
instrument="$source_dir/../../EXP-2026-11-25-1942-inkling-late-prefill-budget/source/instrument.py"
mkdir "$out"
cp "$previous/"{inkling-backend.c,prefill.h,prefill-team.h,graph-execute.c,cuda-driver.c,cuda-driver.reference.merged.c,infer-main.c,bridge.h,bridge-driver.h,bridge-image.h,decode-lfu.h,lfu.goose,link.sh} "$out/"
(cd "$out" && python3 "$instrument")
sed -i 's/-lm -ldl -pthread/-lnvToolsExt -lm -ldl -pthread/' "$out/link.sh"
bash "$out/link.sh"
