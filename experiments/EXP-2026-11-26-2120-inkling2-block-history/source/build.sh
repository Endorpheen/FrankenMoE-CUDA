#!/usr/bin/env bash
set -euo pipefail

baseline=$(realpath "$1")
historical_worktree=$(realpath "$2")
output=$(realpath -m "$3")
recipe=$(cd "$(dirname "$0")" && pwd)
(cd "$baseline" && sha256sum -c "$recipe/../data/reference-sources.sha256")
mkdir "$output"
cp "$baseline/"* "$output/"
patch -d "$output" -p1 < "$recipe/block-history.patch"
cd "$historical_worktree"
export ASSERT_SOURCE_DIR="$(dirname "$recipe")/../../build/inkling2-candidates/2120-block-history/source"
export ASSERT_SOURCE_DIR
ASSERT_SOURCE_DIR=$(realpath -m "$ASSERT_SOURCE_DIR")
bash "$output/link.sh"
(cd "$output" && sha256sum -c "$recipe/../data/executable.sha256")
