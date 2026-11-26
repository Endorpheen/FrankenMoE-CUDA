#!/usr/bin/env bash
set -euo pipefail

historical_worktree=$(realpath "$1")
output=$(realpath -m "$2")
recipe=$(cd "$(dirname "$0")" && pwd)
reference="$historical_worktree/build/checkpoint-inkling-exp1969-before-bonsai"
attention="$historical_worktree/build/inkling-exp-baseline/device-attention"
mkdir "$output"
cp "$reference/source/"* "$output/"
cp "$attention/inkling-moe-tile32/native-kv-driver.h" "$output/"
cp "$attention/inkling-resident-moe/moe-epilogue-driver.h" "$output/"
(cd "$output" && sha256sum -c "$recipe/../data/reference-sources.sha256")
patch -d "$output" -p1 < "$recipe/cumulative.patch"
cp "$recipe/"*-image.h "$output/"
cd "$historical_worktree"
export ASSERT_SOURCE_DIR=/tmp/inkling-goal-600-35/decode-ngram-screen/spec-runtime
bash "$output/link.sh"
(cd "$output" && sha256sum -c "$recipe/../data/executable.sha256")
